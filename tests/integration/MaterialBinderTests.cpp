// SPDX-License-Identifier: MIT
//
// `HOUSE-00162`. The binder's whole reason to exist is that it refuses, at registration, two things
// phase 1 MEASURED to be impossible -- an unlit `SkinnedEffect`, and a bone palette above 72 -- so
// that they fail where the error can name the material rather than two hundred draws later where it
// can only name the effect. Those refusals are what these tests are mostly about.
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include "cnahouse/rendering/MaterialBinder.hpp"
#include "cnahouse/util/Ids.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using cnahouse::rendering::CullPolicy;
    using cnahouse::rendering::DrawParams;
    using cnahouse::rendering::MaterialBinder;
    using cnahouse::rendering::MaterialDesc;
    using cnahouse::rendering::MaterialKind;
    using cnahouse::util::ErrorCode;
    using cnahouse::util::Id;

    void RunWithBinder(const std::function<void(MaterialBinder&)>& body)
    {
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                MaterialBinder binder(device);
                body(binder);
            });
        host.Run();
        EXPECT_TRUE(host.Ran()) << "the frame that does the measuring never ran";
        EXPECT_EQ(host.Failure(), "") << "an effect rejected a parameter the binder set";
    }

    MaterialDesc Wall()
    {
        MaterialDesc desc;
        desc.kind = MaterialKind::DualTexture;
        desc.diffuseTexture = "Textures/wall";
        desc.secondTexture = "Textures/wall_lm";
        return desc;
    }

    MaterialDesc Foliage()
    {
        MaterialDesc desc;
        desc.kind = MaterialKind::AlphaTest;
        desc.diffuseTexture = "Textures/leaf";
        desc.twoSided = true;
        return desc;
    }

    MaterialDesc Pet()
    {
        MaterialDesc desc;
        desc.kind = MaterialKind::Skinned;
        desc.diffuseTexture = "Textures/cat";
        return desc;
    }

    TEST(MaterialBinderTests, ARegisteredMaterialIsFoundAndAnUnregisteredOneIsNot)
    {
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                ASSERT_TRUE(binder.Register(Id::Of("MAT_WALL"), Wall()).HasValue());
                EXPECT_EQ(binder.Count(), 1u);

                const MaterialDesc* found = binder.Find(Id::Of("MAT_WALL"));
                ASSERT_NE(found, nullptr);
                EXPECT_EQ(found->kind, MaterialKind::DualTexture);
                EXPECT_EQ(binder.Find(Id::Of("MAT_NOT_THERE")), nullptr);
            });
    }

    TEST(MaterialBinderTests, ADuplicateIdIsRefusedRatherThanSilentlyReplacing)
    {
        // Silently replacing is the worse failure: two rooms sharing a material name would render
        // correctly for whichever loaded second, which is a bug that reproduces one room at a time.
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                ASSERT_TRUE(binder.Register(Id::Of("MAT_WALL"), Wall()).HasValue());
                const auto second = binder.Register(Id::Of("MAT_WALL"), Wall());
                ASSERT_FALSE(second.HasValue());
                EXPECT_EQ(second.Error().Code(), ErrorCode::Duplicate);
                EXPECT_EQ(binder.Count(), 1u);
            });
    }

    TEST(MaterialBinderTests, AnUnlitSkinnedMaterialIsRefusedAtRegistration)
    {
        // MEASURED (`HOUSE-00077`): `SkinnedEffect` throws "SkinnedEffect does not support setting
        // LightingEnabled to false.", exactly as XNA 4.0 does. Catching it here means the error
        // names the material.
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                MaterialDesc desc = Pet();
                desc.perPixelLighting = false;
                const auto result = binder.Register(Id::Of("MAT_PET"), desc);
                ASSERT_FALSE(result.HasValue());
                EXPECT_EQ(result.Error().Code(), ErrorCode::Unsupported);
                EXPECT_NE(result.Error().Message().find("SkinnedEffect"), std::string::npos)
                    << result.Error().ToString();
            });
    }

    TEST(MaterialBinderTests, ADualTextureMaterialWithoutASecondTextureIsRefused)
    {
        // Left alone it draws black, because the `HOUSE-00078` lightmap product is with nothing --
        // a material that meant to be `Basic`.
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                MaterialDesc desc = Wall();
                desc.secondTexture.clear();
                const auto result = binder.Register(Id::Of("MAT_WALL"), desc);
                ASSERT_FALSE(result.HasValue());
                EXPECT_EQ(result.Error().Code(), ErrorCode::InvalidData);
            });
    }

    TEST(MaterialBinderTests, AnOutOfRangeReferenceAlphaIsRefused)
    {
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                MaterialDesc desc = Foliage();
                desc.referenceAlpha = 256;
                EXPECT_EQ(binder.Register(Id::Of("MAT_LEAF"), desc).Error().Code(), ErrorCode::OutOfRange);

                desc.referenceAlpha = -1;
                EXPECT_EQ(binder.Register(Id::Of("MAT_LEAF"), desc).Error().Code(), ErrorCode::OutOfRange);

                // 0 and 255 are both legal: `HOUSE-00080` measured the cutoff exact to one alpha
                // value, so the ends of the range are usable rather than degenerate.
                desc.referenceAlpha = 0;
                EXPECT_TRUE(binder.Register(Id::Of("MAT_LEAF_0"), desc).HasValue());
                desc.referenceAlpha = 255;
                EXPECT_TRUE(binder.Register(Id::Of("MAT_LEAF_255"), desc).HasValue());
            });
    }

    TEST(MaterialBinderTests, TheZeroIdIsRefusedBecauseItMeansNoMaterial)
    {
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                const auto result = binder.Register(Id(), Wall());
                ASSERT_FALSE(result.HasValue());
                EXPECT_EQ(result.Error().Code(), ErrorCode::InvalidArgument);
            });
    }

    TEST(MaterialBinderTests, OneEffectInstanceIsSharedByEveryMaterialOfAKind)
    {
        // An XNA effect object holds whatever parameters were last written to it, so the parameters
        // are set per draw whatever happens -- an instance per material would buy nothing and cost
        // one shader object per material in the house.
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                MaterialDesc red;
                red.diffuse[0] = 1.0f;
                red.diffuse[1] = 0.0f;
                red.diffuse[2] = 0.0f;
                MaterialDesc blue;
                blue.diffuse[0] = 0.0f;
                blue.diffuse[1] = 0.0f;
                blue.diffuse[2] = 1.0f;
                ASSERT_TRUE(binder.Register(Id::Of("MAT_RED"), red).HasValue());
                ASSERT_TRUE(binder.Register(Id::Of("MAT_BLUE"), blue).HasValue());

                DrawParams draw;
                const auto first = binder.Bind(Id::Of("MAT_RED"), draw);
                ASSERT_TRUE(first.HasValue()) << first.Error().ToString();
                const auto second = binder.Bind(Id::Of("MAT_BLUE"), draw);
                ASSERT_TRUE(second.HasValue()) << second.Error().ToString();

                EXPECT_EQ(*first, *second) << "two Basic materials must share one BasicEffect";
                EXPECT_EQ(binder.EffectsCreated(), 1u);
            });
    }

    TEST(MaterialBinderTests, EachKindGetsItsOwnEffectAndOnlyWhenItIsFirstUsed)
    {
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                ASSERT_TRUE(binder.Register(Id::Of("MAT_WALL"), Wall()).HasValue());
                ASSERT_TRUE(binder.Register(Id::Of("MAT_LEAF"), Foliage()).HasValue());
                EXPECT_EQ(binder.EffectsCreated(), 0u)
                    << "registering a material must not create a shader object";

                DrawParams draw;
                ASSERT_TRUE(binder.Bind(Id::Of("MAT_WALL"), draw).HasValue());
                EXPECT_EQ(binder.EffectsCreated(), 1u);
                ASSERT_TRUE(binder.Bind(Id::Of("MAT_LEAF"), draw).HasValue());
                EXPECT_EQ(binder.EffectsCreated(), 2u);
                ASSERT_TRUE(binder.Bind(Id::Of("MAT_WALL"), draw).HasValue());
                EXPECT_EQ(binder.EffectsCreated(), 2u) << "the second bind reuses the instance";
            });
    }

    TEST(MaterialBinderTests, BindingAnUnknownMaterialIsAnErrorAndNotACrash)
    {
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                DrawParams draw;
                const auto result = binder.Bind(Id::Of("MAT_GHOST"), draw);
                ASSERT_FALSE(result.HasValue());
                EXPECT_EQ(result.Error().Code(), ErrorCode::NotFound);
            });
    }

    TEST(MaterialBinderTests, ASkinnedMaterialNeedsABonePaletteAndAcceptsExactlySeventyTwo)
    {
        // MEASURED (`HOUSE-00077`): `SkinnedEffect::MaxBones == 72`; 72 accepted, 73 throws
        // "boneTransforms exceeds MaxBones.". Reported rather than thrown, because a skin one bone
        // over the limit is a content problem and the frame should say so and keep going.
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                ASSERT_TRUE(binder.Register(Id::Of("MAT_PET"), Pet()).HasValue());

                DrawParams noBones;
                EXPECT_EQ(binder.Bind(Id::Of("MAT_PET"), noBones).Error().Code(), ErrorCode::InvalidArgument);

                std::vector<Microsoft::Xna::Framework::Matrix> palette(
                    MaterialBinder::kMaxBones, Microsoft::Xna::Framework::Matrix::getIdentityProperty());
                DrawParams exact;
                exact.bones = &palette;
                EXPECT_TRUE(binder.Bind(Id::Of("MAT_PET"), exact).HasValue())
                    << "72 is the measured limit and must be accepted";

                palette.push_back(Microsoft::Xna::Framework::Matrix::getIdentityProperty());
                const auto tooMany = binder.Bind(Id::Of("MAT_PET"), exact);
                ASSERT_FALSE(tooMany.HasValue());
                EXPECT_EQ(tooMany.Error().Code(), ErrorCode::OutOfRange);
            });
    }

    TEST(MaterialBinderTests, AShortBonePaletteIsPaddedRatherThanRefused)
    {
        // MEASURED (`HOUSE-00075`): blend indices are SKIN-LOCAL, so slot i is joint i of this skin
        // and the padding beyond the skin's joint count is never referenced. A four-bone pet must
        // not have to ship 72 matrices.
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                ASSERT_TRUE(binder.Register(Id::Of("MAT_PET"), Pet()).HasValue());
                std::vector<Microsoft::Xna::Framework::Matrix> palette(
                    4, Microsoft::Xna::Framework::Matrix::getIdentityProperty());
                DrawParams draw;
                draw.bones = &palette;
                EXPECT_TRUE(binder.Bind(Id::Of("MAT_PET"), draw).HasValue());
            });
    }

    TEST(MaterialBinderTests, TheCullPolicyComesFromTheMaterialAndTheWorldDeterminant)
    {
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                ASSERT_TRUE(binder.Register(Id::Of("MAT_WALL"), Wall()).HasValue());
                ASSERT_TRUE(binder.Register(Id::Of("MAT_LEAF"), Foliage()).HasValue());

                EXPECT_EQ(*binder.CullFor(Id::Of("MAT_WALL"), 1.0f), CullPolicy::ImportedFront);
                EXPECT_EQ(*binder.CullFor(Id::Of("MAT_WALL"), -1.0f), CullPolicy::Mirrored);
                // Two-sided wins: a foliage card is meant to be seen from behind whether or not its
                // placement mirrors.
                EXPECT_EQ(*binder.CullFor(Id::Of("MAT_LEAF"), 1.0f), CullPolicy::TwoSided);
                EXPECT_EQ(*binder.CullFor(Id::Of("MAT_LEAF"), -1.0f), CullPolicy::TwoSided);

                EXPECT_EQ(binder.CullFor(Id::Of("MAT_GHOST"), 1.0f).Error().Code(), ErrorCode::NotFound);
            });
    }
} // namespace
