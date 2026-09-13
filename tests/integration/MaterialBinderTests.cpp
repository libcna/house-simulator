// SPDX-License-Identifier: MIT
//
// `HOUSE-00162`, `HOUSE-00891`, `HOUSE-00892`. The binder refuses, at registration, things phase 1
// MEASURED to be impossible, turns the loaded material table into small draw-time descriptions, and
// writes every requested value to the selected stock XNA effect. A bad table fails before a frame can
// observe it.
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DualTextureEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/rendering/MaterialBinder.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/world/WorldLoader.hpp"
#include "cnahouse/world/WorldTypes.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using cnahouse::rendering::CullPolicy;
    using cnahouse::rendering::DrawParams;
    using cnahouse::rendering::FogParams;
    using cnahouse::rendering::MaterialBinder;
    using cnahouse::rendering::MaterialDesc;
    using cnahouse::rendering::MaterialKind;
    using cnahouse::util::ErrorCode;
    using cnahouse::util::Id;
    using Microsoft::Xna::Framework::Vector3;

    void RunWithDeviceBinder(const std::function<void(Gfx::GraphicsDevice&, MaterialBinder&)>& body)
    {
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                MaterialBinder binder(device);
                body(device, binder);
            });
        host.Run();
        EXPECT_TRUE(host.Ran()) << "the frame that does the measuring never ran";
        EXPECT_EQ(host.Failure(), "") << "an effect rejected a parameter the binder set";
    }

    void RunWithBinder(const std::function<void(MaterialBinder&)>& body)
    {
        RunWithDeviceBinder([&](Gfx::GraphicsDevice&, MaterialBinder& binder) { body(binder); });
    }

    MaterialDesc Wall()
    {
        MaterialDesc desc;
        desc.kind = MaterialKind::DualTexture;
        desc.diffuseTexture = "Textures/wall";
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
                desc.lightingEnabled = false;
                const auto result = binder.Register(Id::Of("MAT_PET"), desc);
                ASSERT_FALSE(result.HasValue());
                EXPECT_EQ(result.Error().Code(), ErrorCode::Unsupported);
                EXPECT_NE(result.Error().Message().find("SkinnedEffect"), std::string::npos)
                    << result.Error().ToString();
            });
    }

    TEST(MaterialBinderTests, TheLoadedDefinitionBecomesTheRegisteredDrawDescription)
    {
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                cnahouse::world::MaterialDef definition;
                definition.id = Id::Of("MAT_LEAF");
                definition.albedo = "Textures/leaf";
                definition.tint = {0.25F, 0.50F, 0.75F};
                definition.alpha = 0.80F;
                definition.specularColor = {0.10F, 0.20F, 0.30F};
                definition.specularPower = 24.0F;
                definition.alphaMode = cnahouse::world::AlphaMode::Mask;
                definition.alphaCutoff = 0.5F;
                definition.twoSided = true;
                definition.effectTierS = cnahouse::world::EffectTier::AlphaTest;

                ASSERT_TRUE(binder.Register(definition).HasValue());
                const MaterialDesc* desc = binder.Find(definition.id);
                ASSERT_NE(desc, nullptr);
                EXPECT_EQ(desc->kind, MaterialKind::AlphaTest);
                EXPECT_EQ(desc->diffuseTexture, "Textures/leaf");
                EXPECT_FLOAT_EQ(desc->diffuse[0], 0.25F);
                EXPECT_FLOAT_EQ(desc->diffuse[1], 0.50F);
                EXPECT_FLOAT_EQ(desc->diffuse[2], 0.75F);
                EXPECT_FLOAT_EQ(desc->alpha, 0.80F);
                EXPECT_FLOAT_EQ(desc->specularColour[0], 0.10F);
                EXPECT_FLOAT_EQ(desc->specularColour[1], 0.20F);
                EXPECT_FLOAT_EQ(desc->specularColour[2], 0.30F);
                EXPECT_FLOAT_EQ(desc->specularPower, 24.0F);
                EXPECT_EQ(desc->referenceAlpha, 128);
                EXPECT_TRUE(desc->twoSided);
            });
    }

    TEST(MaterialBinderTests, TheCompleteAuthoredMaterialTableRegistersByItsPermanentIds)
    {
        // Integration tests run from the build tree, exactly where the game finds deployed data.
        // The `world-content-current` fixture proves this copy matches `assets-src/world` first.
        cnahouse::world::WorldData::Contents contents;
        const auto loaded = cnahouse::world::WorldLoader::LoadMaterials("content/world", contents);
        ASSERT_TRUE(loaded.HasValue()) << loaded.Error().ToString();

        RunWithBinder(
            [&contents](MaterialBinder& binder)
            {
                ASSERT_TRUE(binder.RegisterAll(contents.materials).HasValue());
                EXPECT_EQ(binder.Count(), contents.materials.size());
                EXPECT_EQ(binder.Count(), 20U);

                const MaterialDesc* glass = binder.Find(Id::Of("MAT_GLASS_CLEAR"));
                ASSERT_NE(glass, nullptr);
                EXPECT_EQ(glass->kind, MaterialKind::Basic);
                EXPECT_FLOAT_EQ(glass->alpha, 0.12F);

                const MaterialDesc* lawn = binder.Find(Id::Of("MAT_GROUND_LAWN"));
                ASSERT_NE(lawn, nullptr);
                EXPECT_EQ(lawn->kind, MaterialKind::DualTexture);
                EXPECT_EQ(lawn->diffuseTexture, "Textures/Materials/grass_lawn_albedo");
            });
    }

    TEST(MaterialBinderTests, AnInvalidLoadedAlphaOrCutoffIsRefusedAtTheRegistryBoundary)
    {
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                cnahouse::world::MaterialDef definition;
                definition.id = Id::Of("MAT_BAD");
                definition.alpha = 1.01F;
                EXPECT_EQ(binder.Register(definition).Error().Code(), ErrorCode::OutOfRange);

                definition.alpha = 1.0F;
                definition.alphaMode = cnahouse::world::AlphaMode::Mask;
                definition.alphaCutoff = -0.01F;
                EXPECT_EQ(binder.Register(definition).Error().Code(), ErrorCode::OutOfRange);

                definition.alphaCutoff.reset();
                EXPECT_EQ(binder.Register(definition).Error().Code(), ErrorCode::InvalidData);
                EXPECT_EQ(binder.Count(), 0U);
            });
    }

    TEST(MaterialBinderTests, AFailedTableRegistrationLeavesNoPartialRowsBehind)
    {
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                const Id existing = Id::Of("MAT_EXISTING");
                ASSERT_TRUE(binder.Register(existing, Wall()).HasValue());

                std::vector<cnahouse::world::MaterialDef> definitions(2);
                definitions[0].id = Id::Of("MAT_NEW");
                definitions[1].id = existing;

                const auto result = binder.RegisterAll(definitions);
                ASSERT_FALSE(result.HasValue());
                EXPECT_EQ(result.Error().Code(), ErrorCode::Duplicate);
                EXPECT_EQ(binder.Count(), 1U);
                EXPECT_NE(binder.Find(existing), nullptr);
                EXPECT_EQ(binder.Find(definitions[0].id), nullptr);
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

    TEST(MaterialBinderTests, BasicEffectReceivesEveryMaterialAndFogParameterAndClearsPerDrawState)
    {
        cnahouse::testsupport::DeviceHost host(
            [](Gfx::GraphicsDevice& device)
            {
                MaterialBinder binder(device);
                MaterialDesc desc;
                desc.kind = MaterialKind::Basic;
                desc.diffuse[0] = 0.25F;
                desc.diffuse[1] = 0.50F;
                desc.diffuse[2] = 0.75F;
                desc.alpha = 0.60F;
                desc.specularColour[0] = 0.10F;
                desc.specularColour[1] = 0.20F;
                desc.specularColour[2] = 0.30F;
                desc.specularPower = 27.0F;
                desc.vertexColour = true;
                desc.lightingEnabled = true;
                desc.perPixelLighting = true;
                const Id id = Id::Of("MAT_BASIC_COMPLETE");
                ASSERT_TRUE(binder.Register(id, desc).HasValue());

                Gfx::Texture2D texture(device, 2, 2);
                FogParams fog;
                fog.colour[0] = 0.12F;
                fog.colour[1] = 0.24F;
                fog.colour[2] = 0.36F;
                fog.start = 7.0F;
                fog.end = 43.0F;
                DrawParams draw;
                draw.diffuse = &texture;
                draw.fog = &fog;

                const auto bound = binder.Bind(id, draw);
                ASSERT_TRUE(bound.HasValue()) << bound.Error().ToString();
                auto* effect = static_cast<Gfx::BasicEffect*>(*bound);
                EXPECT_EQ(effect->getDiffuseColorProperty(), Vector3(0.25F, 0.50F, 0.75F));
                EXPECT_FLOAT_EQ(effect->getAlphaProperty(), 0.60F);
                EXPECT_EQ(effect->getSpecularColorProperty(), Vector3(0.10F, 0.20F, 0.30F));
                EXPECT_FLOAT_EQ(effect->getSpecularPowerProperty(), 27.0F);
                EXPECT_TRUE(effect->getVertexColorEnabledProperty());
                EXPECT_TRUE(effect->getLightingEnabledProperty());
                EXPECT_TRUE(effect->getPreferPerPixelLightingProperty());
                EXPECT_TRUE(effect->getTextureEnabledProperty());
                EXPECT_EQ(effect->getTextureProperty(), &texture);
                EXPECT_TRUE(effect->getFogEnabledProperty());
                EXPECT_EQ(effect->getFogColorProperty(), Vector3(0.12F, 0.24F, 0.36F));
                EXPECT_FLOAT_EQ(effect->getFogStartProperty(), 7.0F);
                EXPECT_FLOAT_EQ(effect->getFogEndProperty(), 43.0F);

                ASSERT_TRUE(binder.Bind(id, DrawParams{}).HasValue());
                EXPECT_FALSE(effect->getTextureEnabledProperty());
                EXPECT_FALSE(effect->getFogEnabledProperty());
            });
        host.Run();
        EXPECT_TRUE(host.Ran()) << "the frame that does the measuring never ran";
        EXPECT_EQ(host.Failure(), "") << "BasicEffect rejected a binder parameter";
    }

    TEST(MaterialBinderTests, AnEmissiveDefinitionBecomesAnUnlitBasicMaterial)
    {
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                cnahouse::world::MaterialDef definition;
                definition.id = Id::Of("MAT_EMISSIVE");
                definition.materialClass = cnahouse::world::MaterialClass::Emissive;
                definition.effectTierS = cnahouse::world::EffectTier::Basic;
                ASSERT_TRUE(binder.Register(definition).HasValue());

                const auto bound = binder.Bind(definition.id, DrawParams{});
                ASSERT_TRUE(bound.HasValue()) << bound.Error().ToString();
                auto* effect = static_cast<Gfx::BasicEffect*>(*bound);
                EXPECT_FALSE(effect->getLightingEnabledProperty());
            });
    }

    TEST(MaterialBinderTests, EachKindGetsItsOwnEffectAndOnlyWhenItIsFirstUsed)
    {
        RunWithDeviceBinder(
            [](Gfx::GraphicsDevice& device, MaterialBinder& binder)
            {
                ASSERT_TRUE(binder.Register(Id::Of("MAT_WALL"), Wall()).HasValue());
                ASSERT_TRUE(binder.Register(Id::Of("MAT_LEAF"), Foliage()).HasValue());
                EXPECT_EQ(binder.EffectsCreated(), 0u)
                    << "registering a material must not create a shader object";

                Gfx::Texture2D albedo(device, 2, 2);
                Gfx::Texture2D lightmap(device, 2, 2);
                DrawParams draw;
                draw.diffuse = &albedo;
                draw.lightmap = &lightmap;
                ASSERT_TRUE(binder.Bind(Id::Of("MAT_WALL"), draw).HasValue());
                EXPECT_EQ(binder.EffectsCreated(), 1u);
                ASSERT_TRUE(binder.Bind(Id::Of("MAT_LEAF"), draw).HasValue());
                EXPECT_EQ(binder.EffectsCreated(), 2u);
                ASSERT_TRUE(binder.Bind(Id::Of("MAT_WALL"), draw).HasValue());
                EXPECT_EQ(binder.EffectsCreated(), 2u) << "the second bind reuses the instance";
            });
    }

    TEST(MaterialBinderTests, DualTextureEffectReceivesAlbedoLightmapTintAndFog)
    {
        RunWithDeviceBinder(
            [](Gfx::GraphicsDevice& device, MaterialBinder& binder)
            {
                MaterialDesc desc = Wall();
                desc.diffuse[0] = 0.20F;
                desc.diffuse[1] = 0.40F;
                desc.diffuse[2] = 0.60F;
                desc.alpha = 0.75F;
                desc.vertexColour = true;
                const Id id = Id::Of("MAT_LIT_WALL");
                ASSERT_TRUE(binder.Register(id, desc).HasValue());

                Gfx::Texture2D albedo(device, 2, 2);
                Gfx::Texture2D lightmap(device, 2, 2);
                FogParams fog;
                fog.colour[0] = 0.10F;
                fog.colour[1] = 0.15F;
                fog.colour[2] = 0.20F;
                fog.start = 8.0F;
                fog.end = 48.0F;
                DrawParams draw;
                draw.diffuse = &albedo;
                draw.lightmap = &lightmap;
                draw.fog = &fog;

                const auto bound = binder.Bind(id, draw);
                ASSERT_TRUE(bound.HasValue()) << bound.Error().ToString();
                auto* effect = static_cast<Gfx::DualTextureEffect*>(*bound);
                EXPECT_EQ(effect->getTextureProperty(), &albedo);
                EXPECT_EQ(effect->getTexture2Property(), &lightmap);
                EXPECT_EQ(effect->getDiffuseColorProperty(), Vector3(0.20F, 0.40F, 0.60F));
                EXPECT_FLOAT_EQ(effect->getAlphaProperty(), 0.75F);
                EXPECT_TRUE(effect->getVertexColorEnabledProperty());
                EXPECT_TRUE(effect->getFogEnabledProperty());
                EXPECT_EQ(effect->getFogColorProperty(), Vector3(0.10F, 0.15F, 0.20F));
                EXPECT_FLOAT_EQ(effect->getFogStartProperty(), 8.0F);
                EXPECT_FLOAT_EQ(effect->getFogEndProperty(), 48.0F);

                draw.fog = nullptr;
                ASSERT_TRUE(binder.Bind(id, draw).HasValue());
                EXPECT_FALSE(effect->getFogEnabledProperty());
            });
    }

    TEST(MaterialBinderTests, DualTextureEffectRefusesEitherMissingTexture)
    {
        RunWithDeviceBinder(
            [](Gfx::GraphicsDevice& device, MaterialBinder& binder)
            {
                const Id id = Id::Of("MAT_LIT_WALL");
                ASSERT_TRUE(binder.Register(id, Wall()).HasValue());
                Gfx::Texture2D texture(device, 2, 2);

                DrawParams noLightmap;
                noLightmap.diffuse = &texture;
                EXPECT_EQ(binder.Bind(id, noLightmap).Error().Code(), ErrorCode::InvalidArgument);

                DrawParams noAlbedo;
                noAlbedo.lightmap = &texture;
                EXPECT_EQ(binder.Bind(id, noAlbedo).Error().Code(), ErrorCode::InvalidArgument);
                EXPECT_EQ(binder.EffectsCreated(), 0U)
                    << "an invalid draw must fail before allocating the shared effect";
            });
    }

    TEST(MaterialBinderTests, AlphaTestEffectReceivesTextureCutoffTintAndFog)
    {
        RunWithDeviceBinder(
            [](Gfx::GraphicsDevice& device, MaterialBinder& binder)
            {
                MaterialDesc desc = Foliage();
                desc.diffuse[0] = 0.25F;
                desc.diffuse[1] = 0.50F;
                desc.diffuse[2] = 0.75F;
                desc.alpha = 0.625F;
                desc.vertexColour = true;
                desc.referenceAlpha = 173;
                const Id id = Id::Of("MAT_ALPHA_LEAF");
                ASSERT_TRUE(binder.Register(id, desc).HasValue());

                Gfx::Texture2D albedo(device, 2, 2);
                FogParams fog;
                fog.colour[0] = 0.15F;
                fog.colour[1] = 0.20F;
                fog.colour[2] = 0.25F;
                fog.start = 12.0F;
                fog.end = 72.0F;
                DrawParams draw;
                draw.diffuse = &albedo;
                draw.fog = &fog;

                const auto bound = binder.Bind(id, draw);
                ASSERT_TRUE(bound.HasValue()) << bound.Error().ToString();
                auto* effect = static_cast<Gfx::AlphaTestEffect*>(*bound);
                EXPECT_EQ(effect->getTextureProperty(), &albedo);
                EXPECT_EQ(effect->getDiffuseColorProperty(), Vector3(0.25F, 0.50F, 0.75F));
                EXPECT_FLOAT_EQ(effect->getAlphaProperty(), 0.625F);
                EXPECT_TRUE(effect->getVertexColorEnabledProperty());
                EXPECT_EQ(effect->getAlphaFunctionProperty(), Gfx::CompareFunction::Greater);
                EXPECT_EQ(effect->getReferenceAlphaProperty(), 173);
                EXPECT_TRUE(effect->getFogEnabledProperty());
                EXPECT_EQ(effect->getFogColorProperty(), Vector3(0.15F, 0.20F, 0.25F));
                EXPECT_FLOAT_EQ(effect->getFogStartProperty(), 12.0F);
                EXPECT_FLOAT_EQ(effect->getFogEndProperty(), 72.0F);

                EXPECT_EQ(*binder.CullFor(id, 1.0F), CullPolicy::TwoSided);
                EXPECT_EQ(*binder.CullFor(id, -1.0F), CullPolicy::TwoSided);

                draw.fog = nullptr;
                ASSERT_TRUE(binder.Bind(id, draw).HasValue());
                EXPECT_FALSE(effect->getFogEnabledProperty());
            });
    }

    TEST(MaterialBinderTests, AlphaTestEffectRefusesAMissingTextureBeforeAllocation)
    {
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                const Id id = Id::Of("MAT_ALPHA_LEAF");
                ASSERT_TRUE(binder.Register(id, Foliage()).HasValue());

                const auto bound = binder.Bind(id, DrawParams{});
                ASSERT_FALSE(bound.HasValue());
                EXPECT_EQ(bound.Error().Code(), ErrorCode::InvalidArgument);
                EXPECT_EQ(binder.EffectsCreated(), 0U);
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
