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
#include "Microsoft/Xna/Framework/Graphics/EnvironmentMapEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureCube.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/rendering/MaterialBinder.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/world/ChunkReader.hpp"
#include "cnahouse/world/WorldLoader.hpp"
#include "cnahouse/world/WorldTypes.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using cnahouse::rendering::CullPolicy;
    using cnahouse::rendering::DrawParams;
    using cnahouse::rendering::EnvironmentMapParams;
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
                EXPECT_EQ(binder.Count(), 184U)
                    << "HOUSE-01285 adds the family reading-lamp switched-emitter role";
                const MaterialDesc* entryPanel = binder.Find(Id::Of("MAT_EXTERIOR_DOOR_PANEL_HARDWOOD"));
                ASSERT_NE(entryPanel, nullptr);
                EXPECT_EQ(entryPanel->kind, MaterialKind::Basic);
                EXPECT_TRUE(entryPanel->lightingEnabled);
                const MaterialDesc* entryHardware = binder.Find(Id::Of("MAT_EXTERIOR_DOOR_HARDWARE_BRONZE"));
                ASSERT_NE(entryHardware, nullptr);
                EXPECT_EQ(entryHardware->kind, MaterialKind::Basic);
                EXPECT_TRUE(entryHardware->lightingEnabled);
                const MaterialDesc* garageDoor = binder.Find(Id::Of("MAT_EXTERIOR_DOOR_GARAGE_PAINTED"));
                ASSERT_NE(garageDoor, nullptr);
                EXPECT_EQ(garageDoor->kind, MaterialKind::Basic);
                EXPECT_EQ(garageDoor->diffuseTexture, "Textures/Materials/paint_white_fine_albedo");
                const MaterialDesc* garagePanel =
                    binder.Find(Id::Of("MAT_EXTERIOR_DOOR_GARAGE_PANEL_PAINTED"));
                ASSERT_NE(garagePanel, nullptr);
                EXPECT_EQ(garagePanel->kind, MaterialKind::Basic);
                EXPECT_EQ(garagePanel->diffuseTexture, garageDoor->diffuseTexture);
                EXPECT_GT(garagePanel->diffuse[0], garageDoor->diffuse[0]);
                const MaterialDesc* fixtureMetal = binder.Find(Id::Of("MAT_FIXTURE_DARK_BRONZE"));
                ASSERT_NE(fixtureMetal, nullptr);
                EXPECT_EQ(fixtureMetal->kind, MaterialKind::Basic);
                EXPECT_TRUE(fixtureMetal->lightingEnabled);
                const MaterialDesc* fixtureShade = binder.Find(Id::Of("MAT_FIXTURE_EMISSIVE_WARM"));
                ASSERT_NE(fixtureShade, nullptr);
                EXPECT_EQ(fixtureShade->kind, MaterialKind::Basic);
                EXPECT_FALSE(fixtureShade->lightingEnabled);
                const MaterialDesc* fixtureFloodLens = binder.Find(Id::Of("MAT_FIXTURE_EMISSIVE_NEUTRAL"));
                ASSERT_NE(fixtureFloodLens, nullptr);
                EXPECT_EQ(fixtureFloodLens->kind, MaterialKind::Basic);
                EXPECT_FALSE(fixtureFloodLens->lightingEnabled);
                const MaterialDesc* diningFabric = binder.Find(Id::Of("MAT_FURNITURE_DINING_UPHOLSTERY"));
                ASSERT_NE(diningFabric, nullptr);
                EXPECT_EQ(diningFabric->kind, MaterialKind::Basic);
                EXPECT_TRUE(diningFabric->lightingEnabled);
                const MaterialDesc* kitchenCounter = binder.Find(Id::Of("MAT_KITCHEN_COUNTER_STONE"));
                ASSERT_NE(kitchenCounter, nullptr);
                EXPECT_EQ(kitchenCounter->kind, MaterialKind::Basic);
                const MaterialDesc* kitchenSteel = binder.Find(Id::Of("MAT_KITCHEN_HARDWARE_STEEL"));
                ASSERT_NE(kitchenSteel, nullptr);
                EXPECT_EQ(kitchenSteel->kind, MaterialKind::Basic);
                const MaterialDesc* kitchenTile = binder.Find(Id::Of("MAT_KITCHEN_BACKSPLASH_TILE"));
                ASSERT_NE(kitchenTile, nullptr);
                EXPECT_EQ(kitchenTile->kind, MaterialKind::Basic);
                const MaterialDesc* ovenGlass = binder.Find(Id::Of("MAT_KITCHEN_OVEN_GLASS"));
                ASSERT_NE(ovenGlass, nullptr);
                EXPECT_EQ(ovenGlass->kind, MaterialKind::Basic);
                const MaterialDesc* kitchenProduce = binder.Find(Id::Of("MAT_KITCHEN_PRODUCE_LEMON"));
                ASSERT_NE(kitchenProduce, nullptr);
                EXPECT_EQ(kitchenProduce->kind, MaterialKind::Basic);
                EXPECT_GT(kitchenProduce->diffuse[0], kitchenProduce->diffuse[2]);
                const MaterialDesc* outdoorRoof = binder.Find(Id::Of("MAT_OUTDOOR_ROOF"));
                ASSERT_NE(outdoorRoof, nullptr);
                EXPECT_EQ(outdoorRoof->kind, MaterialKind::Basic);
                const MaterialDesc* outdoorSiding = binder.Find(Id::Of("MAT_OUTDOOR_SIDING"));
                ASSERT_NE(outdoorSiding, nullptr);
                EXPECT_EQ(outdoorSiding->kind, MaterialKind::Basic);
                EXPECT_EQ(outdoorSiding->diffuseTexture, "Textures/Materials/wood_board_albedo");

                const MaterialDesc* upholstery = binder.Find(Id::Of("MAT_FURNITURE_WHITE_ROOM_PALETTE"));
                ASSERT_NE(upholstery, nullptr);
                EXPECT_EQ(upholstery->kind, MaterialKind::Basic);
                const MaterialDesc* foyerConsole = binder.Find(Id::Of("MAT_FURNITURE_FOYER_CONSOLE"));
                ASSERT_NE(foyerConsole, nullptr);
                EXPECT_EQ(foyerConsole->kind, MaterialKind::Basic);
                const MaterialDesc* foyerArmchair = binder.Find(Id::Of("MAT_FURNITURE_FOYER_ARMCHAIR"));
                ASSERT_NE(foyerArmchair, nullptr);
                EXPECT_EQ(foyerArmchair->kind, MaterialKind::Basic);
                const MaterialDesc* pianoWood = binder.Find(Id::Of("MAT_FURNITURE_PIANO_WOOD"));
                ASSERT_NE(pianoWood, nullptr);
                EXPECT_EQ(pianoWood->kind, MaterialKind::Basic);
                EXPECT_EQ(pianoWood->diffuseTexture, "Textures/Materials/wood_board_albedo");
                const MaterialDesc* pianoIvory = binder.Find(Id::Of("MAT_FURNITURE_PIANO_IVORY"));
                ASSERT_NE(pianoIvory, nullptr);
                EXPECT_EQ(pianoIvory->kind, MaterialKind::Basic);
                const MaterialDesc* pianoBlack = binder.Find(Id::Of("MAT_FURNITURE_PIANO_EBONITE"));
                ASSERT_NE(pianoBlack, nullptr);
                EXPECT_EQ(pianoBlack->kind, MaterialKind::Basic);
                EXPECT_LT(pianoBlack->diffuse[0], pianoIvory->diffuse[0]);
                const MaterialDesc* pianoBrass = binder.Find(Id::Of("MAT_FURNITURE_PIANO_BRASS"));
                ASSERT_NE(pianoBrass, nullptr);
                EXPECT_EQ(pianoBrass->kind, MaterialKind::Basic);
                const MaterialDesc* leaves = binder.Find(Id::Of("MAT_FURNITURE_PLANT_LEAF"));
                ASSERT_NE(leaves, nullptr);
                EXPECT_EQ(leaves->kind, MaterialKind::AlphaTest);
                const MaterialDesc* treeLeaves = binder.Find(Id::Of("MAT_VEGETATION_TREE_FOLIAGE"));
                ASSERT_NE(treeLeaves, nullptr);
                EXPECT_EQ(treeLeaves->kind, MaterialKind::AlphaTest);
                EXPECT_EQ(treeLeaves->diffuseTexture, "Textures/Vegetation/tree_foliage");
                EXPECT_EQ(treeLeaves->referenceAlpha, 115);
                EXPECT_TRUE(treeLeaves->twoSided);
                const MaterialDesc* bark = binder.Find(Id::Of("MAT_VEGETATION_BARK"));
                ASSERT_NE(bark, nullptr);
                EXPECT_EQ(bark->kind, MaterialKind::Basic);

                const MaterialDesc* glass = binder.Find(Id::Of("MAT_GLASS_CLEAR"));
                ASSERT_NE(glass, nullptr);
                EXPECT_EQ(glass->kind, MaterialKind::Basic);
                EXPECT_FLOAT_EQ(glass->alpha, 0.12F);

                const MaterialDesc* outsideFrame = binder.Find(Id::Of("MAT_WINDOW_FRAME_WHITE"));
                ASSERT_NE(outsideFrame, nullptr);
                EXPECT_EQ(outsideFrame->kind, MaterialKind::Basic);
                EXPECT_EQ(outsideFrame->diffuseTexture, "Textures/Materials/paint_white_fine_albedo");
                const MaterialDesc* outsideShutter = binder.Find(Id::Of("MAT_WINDOW_SHUTTER_BLACK"));
                ASSERT_NE(outsideShutter, nullptr);
                EXPECT_EQ(outsideShutter->kind, MaterialKind::Basic);
                EXPECT_EQ(outsideShutter->diffuseTexture, "Textures/Materials/paint_white_fine_albedo");
                EXPECT_LT(outsideShutter->diffuse[0], 0.15F);
                const MaterialDesc* outsideClear = binder.Find(Id::Of("MAT_WINDOW_GLASS_CLEAR"));
                ASSERT_NE(outsideClear, nullptr);
                EXPECT_EQ(outsideClear->kind, MaterialKind::Basic);
                EXPECT_FLOAT_EQ(outsideClear->alpha, glass->alpha);
                const MaterialDesc* outsideObscured = binder.Find(Id::Of("MAT_WINDOW_GLASS_OBSCURED"));
                ASSERT_NE(outsideObscured, nullptr);
                EXPECT_EQ(outsideObscured->kind, MaterialKind::Basic);
                EXPECT_FLOAT_EQ(outsideObscured->alpha, 0.32F);

                const MaterialDesc* lawn = binder.Find(Id::Of("MAT_GROUND_LAWN"));
                ASSERT_NE(lawn, nullptr);
                EXPECT_EQ(lawn->kind, MaterialKind::DualTexture);
                EXPECT_EQ(lawn->diffuseTexture, "Textures/Materials/grass_lawn_albedo");

                const MaterialDesc* chrome = binder.Find(Id::Of("MAT_BASE_METAL_CHROME"));
                ASSERT_NE(chrome, nullptr);
                EXPECT_EQ(chrome->kind, MaterialKind::Basic);
                EXPECT_FLOAT_EQ(chrome->diffuse[0], 0.0F);
                EXPECT_FLOAT_EQ(chrome->specularColour[0], 0.987504F);
                EXPECT_FLOAT_EQ(chrome->specularPower, 256.0F);

                const MaterialDesc* wall = binder.Find(Id::Of("MAT_PAINT_WARM_WHITE"));
                ASSERT_NE(wall, nullptr);
                EXPECT_EQ(wall->kind, MaterialKind::DualTexture);
                EXPECT_EQ(wall->diffuseTexture, "Textures/Materials/paint_white_fine_albedo");
                EXPECT_FLOAT_EQ(wall->diffuse[1], 0.96F);

                const MaterialDesc* vinyl = binder.Find(Id::Of("MAT_FLOOR_VINYL"));
                ASSERT_NE(vinyl, nullptr);
                EXPECT_EQ(vinyl->kind, MaterialKind::DualTexture);
                EXPECT_EQ(vinyl->diffuseTexture, "Textures/Materials/wood_light_floor_albedo");
                EXPECT_FLOAT_EQ(vinyl->diffuse[0], 0.90F);

                const MaterialDesc* roof = binder.Find(Id::Of("MAT_ROOF_SHINGLE"));
                ASSERT_NE(roof, nullptr);
                EXPECT_EQ(roof->kind, MaterialKind::DualTexture);
                EXPECT_EQ(roof->diffuseTexture, "Textures/Materials/tile_light_square_albedo");
                EXPECT_FLOAT_EQ(roof->diffuse[2], 0.74F);

                const MaterialDesc* siding = binder.Find(Id::Of("MAT_SIDING_SAGE"));
                ASSERT_NE(siding, nullptr);
                EXPECT_EQ(siding->kind, MaterialKind::DualTexture);
                EXPECT_EQ(siding->diffuseTexture, "Textures/Materials/wood_board_albedo");
                EXPECT_FLOAT_EQ(siding->diffuse[1], 0.78F);

                const MaterialDesc* flow = binder.Find(Id::Of("MAT_WATER_FLOW"));
                ASSERT_NE(flow, nullptr);
                EXPECT_EQ(flow->kind, MaterialKind::Basic);
                EXPECT_EQ(flow->diffuseTexture, "Textures/Materials/water_flow_albedo");
                EXPECT_FLOAT_EQ(flow->alpha, 0.52F);

                const MaterialDesc* shower = binder.Find(Id::Of("MAT_GLASS_SHOWER"));
                ASSERT_NE(shower, nullptr);
                EXPECT_EQ(shower->kind, MaterialKind::Basic);
                EXPECT_TRUE(shower->diffuseTexture.empty());
                EXPECT_FLOAT_EQ(shower->alpha, 0.16F);

                const MaterialDesc* wetDeck = binder.Find(Id::Of("MAT_DECK_WOOD_WET"));
                ASSERT_NE(wetDeck, nullptr);
                EXPECT_EQ(wetDeck->kind, MaterialKind::DualTexture);
                EXPECT_EQ(wetDeck->diffuseTexture, "Textures/Materials/wood_board_albedo");
                EXPECT_FLOAT_EQ(wetDeck->diffuse[0], 0.4464F);
                EXPECT_FLOAT_EQ(wetDeck->specularColour[0], 0.096F);
                EXPECT_FLOAT_EQ(wetDeck->specularPower, 51.903F);

                const MaterialDesc* snowMetal = binder.Find(Id::Of("MAT_SNOW_METAL"));
                ASSERT_NE(snowMetal, nullptr);
                EXPECT_EQ(snowMetal->kind, MaterialKind::Basic);
                EXPECT_EQ(snowMetal->diffuseTexture, "Textures/Materials/snow_shell_albedo");
                EXPECT_FLOAT_EQ(snowMetal->diffuse[0], 0.94F);
                EXPECT_FLOAT_EQ(snowMetal->alpha, 1.0F);
                EXPECT_FLOAT_EQ(snowMetal->specularColour[0], 0.14F);
                EXPECT_FLOAT_EQ(snowMetal->specularPower, 12.0F);
            });
    }

    TEST(MaterialBinderTests, EveryPlayableStaticChunkHasTheEffectItsVerticesActuallyPack)
    {
        cnahouse::world::WorldData::Contents contents;
        const auto loaded = cnahouse::world::WorldLoader::LoadMaterials("content/world", contents);
        ASSERT_TRUE(loaded.HasValue()) << loaded.Error().ToString();
        const auto chunks = cnahouse::world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
        ASSERT_TRUE(chunks.HasValue()) << chunks.Error().ToString();
        ASSERT_GT(chunks->chunks.size(), 100U);

        RunWithBinder(
            [&contents, &chunks](MaterialBinder& binder)
            {
                ASSERT_TRUE(binder.RegisterAll(contents.materials).HasValue());
                for (const cnahouse::world::Chunk& chunk : chunks->chunks)
                {
                    ASSERT_LT(chunk.material, chunks->materials.size());
                    const std::string& name = chunks->materials[chunk.material];
                    SCOPED_TRACE(name);
                    const MaterialDesc* desc = binder.Find(Id::Of(name));
                    ASSERT_NE(desc, nullptr) << "production must not silently skip a static chunk";
                    switch (chunk.layout)
                    {
                        case cnahouse::world::ChunkLayout::Basic:
                            EXPECT_EQ(desc->kind, MaterialKind::Basic);
                            break;
                        case cnahouse::world::ChunkLayout::Dual:
                            EXPECT_EQ(desc->kind, MaterialKind::DualTexture);
                            break;
                        case cnahouse::world::ChunkLayout::AlphaTest:
                            EXPECT_EQ(desc->kind, MaterialKind::AlphaTest);
                            break;
                    }
                }
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
                draw.colourMultiplier = Vector3(0.50F, 0.25F, 1.0F);
                draw.ambientLight = Vector3(0.05F, 0.06F, 0.07F);
                draw.directionalLights[0] = cnahouse::rendering::StockDirectionalLight{
                    Vector3(0.0F, -1.0F, 0.0F), Vector3(0.20F, 0.30F, 0.40F), Vector3(0.01F, 0.02F, 0.03F)};
                draw.directionalLights[1] = cnahouse::rendering::StockDirectionalLight{
                    Vector3(1.0F, 0.0F, 0.0F), Vector3(0.05F, 0.06F, 0.07F), Vector3()};
                draw.directionalLights[2] = cnahouse::rendering::StockDirectionalLight{
                    Vector3(0.0F, 1.0F, 0.0F), Vector3(0.02F, 0.03F, 0.04F), Vector3()};

                const auto bound = binder.Bind(id, draw);
                ASSERT_TRUE(bound.HasValue()) << bound.Error().ToString();
                auto* effect = static_cast<Gfx::BasicEffect*>(*bound);
                EXPECT_EQ(effect->getDiffuseColorProperty(), Vector3(0.125F, 0.125F, 0.75F));
                EXPECT_FLOAT_EQ(effect->getAlphaProperty(), 0.60F);
                EXPECT_EQ(effect->getSpecularColorProperty(), Vector3(0.10F, 0.20F, 0.30F));
                EXPECT_FLOAT_EQ(effect->getSpecularPowerProperty(), 27.0F);
                EXPECT_TRUE(effect->getVertexColorEnabledProperty());
                EXPECT_TRUE(effect->getLightingEnabledProperty());
                EXPECT_TRUE(effect->getPreferPerPixelLightingProperty());
                EXPECT_EQ(effect->getAmbientLightColorProperty(), Vector3(0.05F, 0.06F, 0.07F));
                EXPECT_TRUE(effect->getDirectionalLight0Property().getEnabledProperty());
                EXPECT_EQ(effect->getDirectionalLight0Property().getDirectionProperty(),
                          Vector3(0.0F, -1.0F, 0.0F));
                EXPECT_EQ(effect->getDirectionalLight0Property().getDiffuseColorProperty(),
                          Vector3(0.20F, 0.30F, 0.40F));
                EXPECT_EQ(effect->getDirectionalLight0Property().getSpecularColorProperty(),
                          Vector3(0.01F, 0.02F, 0.03F));
                EXPECT_TRUE(effect->getDirectionalLight1Property().getEnabledProperty());
                EXPECT_EQ(effect->getDirectionalLight1Property().getDirectionProperty(),
                          Vector3(1.0F, 0.0F, 0.0F));
                EXPECT_TRUE(effect->getDirectionalLight2Property().getEnabledProperty());
                EXPECT_EQ(effect->getDirectionalLight2Property().getDiffuseColorProperty(),
                          Vector3(0.02F, 0.03F, 0.04F));
                EXPECT_TRUE(effect->getTextureEnabledProperty());
                EXPECT_EQ(effect->getTextureProperty(), &texture);
                EXPECT_TRUE(effect->getFogEnabledProperty());
                EXPECT_EQ(effect->getFogColorProperty(), Vector3(0.12F, 0.24F, 0.36F));
                EXPECT_FLOAT_EQ(effect->getFogStartProperty(), 7.0F);
                EXPECT_FLOAT_EQ(effect->getFogEndProperty(), 43.0F);

                ASSERT_TRUE(binder.Bind(id, DrawParams{}).HasValue());
                EXPECT_EQ(effect->getDiffuseColorProperty(), Vector3(0.25F, 0.50F, 0.75F));
                EXPECT_EQ(effect->getAmbientLightColorProperty(), Vector3());
                EXPECT_FALSE(effect->getDirectionalLight0Property().getEnabledProperty())
                    << "a later draw without a cell key must not inherit BasicEffect's white light";
                EXPECT_FALSE(effect->getDirectionalLight1Property().getEnabledProperty());
                EXPECT_FALSE(effect->getDirectionalLight2Property().getEnabledProperty());
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

    TEST(MaterialBinderTests, FiveEffectClassesAreLazyAndReusedAcrossMaterialAndDrawVariants)
    {
        RunWithDeviceBinder(
            [](Gfx::GraphicsDevice& device, MaterialBinder& binder)
            {
                const Id basicA = Id::Of("MAT_POOL_BASIC_A");
                const Id basicB = Id::Of("MAT_POOL_BASIC_B");
                const Id dualA = Id::Of("MAT_POOL_DUAL_A");
                const Id dualB = Id::Of("MAT_POOL_DUAL_B");
                const Id alphaA = Id::Of("MAT_POOL_ALPHA_A");
                const Id alphaB = Id::Of("MAT_POOL_ALPHA_B");
                const Id skinA = Id::Of("MAT_POOL_SKIN_A");
                const Id skinB = Id::Of("MAT_POOL_SKIN_B");

                MaterialDesc first;
                first.diffuse[0] = 0.25F;
                MaterialDesc second;
                second.diffuse[2] = 0.25F;
                ASSERT_TRUE(binder.Register(basicA, first).HasValue());
                ASSERT_TRUE(binder.Register(basicB, second).HasValue());
                first.kind = MaterialKind::DualTexture;
                second.kind = MaterialKind::DualTexture;
                ASSERT_TRUE(binder.Register(dualA, first).HasValue());
                ASSERT_TRUE(binder.Register(dualB, second).HasValue());
                first.kind = MaterialKind::AlphaTest;
                second.kind = MaterialKind::AlphaTest;
                second.referenceAlpha = 192;
                ASSERT_TRUE(binder.Register(alphaA, first).HasValue());
                ASSERT_TRUE(binder.Register(alphaB, second).HasValue());
                first.kind = MaterialKind::Skinned;
                second.kind = MaterialKind::Skinned;
                ASSERT_TRUE(binder.Register(skinA, first).HasValue());
                ASSERT_TRUE(binder.Register(skinB, second).HasValue());
                EXPECT_EQ(binder.EffectsCreated(), 0U);

                Gfx::Texture2D albedoA(device, 2, 2);
                Gfx::Texture2D albedoB(device, 2, 2);
                Gfx::Texture2D lightmapA(device, 2, 2);
                Gfx::Texture2D lightmapB(device, 2, 2);
                Gfx::TextureCube cubeA(device, 4, false, Gfx::SurfaceFormat::Color);
                Gfx::TextureCube cubeB(device, 4, false, Gfx::SurfaceFormat::Color);
                std::vector<Microsoft::Xna::Framework::Matrix> bonesA(
                    1, Microsoft::Xna::Framework::Matrix::getIdentityProperty());
                std::vector<Microsoft::Xna::Framework::Matrix> bonesB(
                    2, Microsoft::Xna::Framework::Matrix::getIdentityProperty());
                DrawParams drawA;
                drawA.diffuse = &albedoA;
                drawA.lightmap = &lightmapA;
                drawA.bones = &bonesA;
                DrawParams drawB;
                drawB.diffuse = &albedoB;
                drawB.lightmap = &lightmapB;
                drawB.bones = &bonesB;

                const auto basicFirst = binder.Bind(basicA, drawA);
                const auto basicSecond = binder.Bind(basicB, drawB);
                ASSERT_TRUE(basicFirst.HasValue());
                ASSERT_TRUE(basicSecond.HasValue());
                EXPECT_EQ(*basicFirst, *basicSecond);
                EXPECT_EQ(binder.EffectsCreated(), 1U);

                const auto dualFirst = binder.Bind(dualA, drawA);
                const auto dualSecond = binder.Bind(dualB, drawB);
                ASSERT_TRUE(dualFirst.HasValue());
                ASSERT_TRUE(dualSecond.HasValue());
                EXPECT_EQ(*dualFirst, *dualSecond);
                EXPECT_EQ(binder.EffectsCreated(), 2U);

                const auto alphaFirst = binder.Bind(alphaA, drawA);
                const auto alphaSecond = binder.Bind(alphaB, drawB);
                ASSERT_TRUE(alphaFirst.HasValue());
                ASSERT_TRUE(alphaSecond.HasValue());
                EXPECT_EQ(*alphaFirst, *alphaSecond);
                EXPECT_EQ(binder.EffectsCreated(), 3U);

                const auto skinFirst = binder.Bind(skinA, drawA);
                const auto skinSecond = binder.Bind(skinB, drawB);
                ASSERT_TRUE(skinFirst.HasValue());
                ASSERT_TRUE(skinSecond.HasValue());
                EXPECT_EQ(*skinFirst, *skinSecond);
                EXPECT_EQ(binder.EffectsCreated(), 4U);

                EnvironmentMapParams environmentA;
                environmentA.cubeMap = &cubeA;
                EnvironmentMapParams environmentB;
                environmentB.cubeMap = &cubeB;
                environmentB.amount = 0.5F;
                environmentB.fresnelFactor = 4.0F;
                const auto environmentFirst = binder.BindEnvironmentMap(basicA, drawA, environmentA);
                const auto environmentSecond = binder.BindEnvironmentMap(basicB, drawB, environmentB);
                ASSERT_TRUE(environmentFirst.HasValue());
                ASSERT_TRUE(environmentSecond.HasValue());
                EXPECT_EQ(*environmentFirst, *environmentSecond);
                EXPECT_EQ(binder.EffectsCreated(), 5U);

                ASSERT_TRUE(binder.Bind(basicA, drawB).HasValue());
                ASSERT_TRUE(binder.Bind(dualA, drawB).HasValue());
                ASSERT_TRUE(binder.Bind(alphaA, drawB).HasValue());
                ASSERT_TRUE(binder.Bind(skinA, drawB).HasValue());
                ASSERT_TRUE(binder.BindEnvironmentMap(basicA, drawB, environmentA).HasValue());
                EXPECT_EQ(binder.EffectsCreated(), 5U)
                    << "a second sweep may overwrite values but must allocate no effect";
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
                draw.colourMultiplier = Vector3(0.50F, 0.25F, 1.0F);

                const auto bound = binder.Bind(id, draw);
                ASSERT_TRUE(bound.HasValue()) << bound.Error().ToString();
                auto* effect = static_cast<Gfx::DualTextureEffect*>(*bound);
                EXPECT_EQ(effect->getTextureProperty(), &albedo);
                EXPECT_EQ(effect->getTexture2Property(), &lightmap);
                EXPECT_EQ(effect->getDiffuseColorProperty(), Vector3(0.10F, 0.10F, 0.60F));
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

    TEST(MaterialBinderTests, EnvironmentMapPassReceivesBakedCubeMaterialAndFog)
    {
        RunWithDeviceBinder(
            [](Gfx::GraphicsDevice& device, MaterialBinder& binder)
            {
                MaterialDesc desc;
                desc.kind = MaterialKind::Basic;
                desc.diffuse[0] = 0.20F;
                desc.diffuse[1] = 0.35F;
                desc.diffuse[2] = 0.50F;
                desc.alpha = 0.70F;
                desc.specularColour[0] = 0.80F;
                desc.specularColour[1] = 0.60F;
                desc.specularColour[2] = 0.40F;
                const Id id = Id::Of("MAT_CHROME");
                ASSERT_TRUE(binder.Register(id, desc).HasValue());

                Gfx::Texture2D albedo(device, 2, 2);
                Gfx::TextureCube cube(device, 4, false, Gfx::SurfaceFormat::Color);
                FogParams fog;
                fog.colour[0] = 0.08F;
                fog.colour[1] = 0.12F;
                fog.colour[2] = 0.16F;
                fog.start = 11.0F;
                fog.end = 71.0F;
                DrawParams draw;
                draw.diffuse = &albedo;
                draw.fog = &fog;
                EnvironmentMapParams environment;
                environment.cubeMap = &cube;
                environment.amount = 0.65F;
                environment.fresnelFactor = 3.0F;

                const auto bound = binder.BindEnvironmentMap(id, draw, environment);
                ASSERT_TRUE(bound.HasValue()) << bound.Error().ToString();
                auto* effect = static_cast<Gfx::EnvironmentMapEffect*>(*bound);
                EXPECT_EQ(effect->getTextureProperty(), &albedo);
                EXPECT_EQ(effect->getEnvironmentMapProperty(), &cube);
                EXPECT_EQ(effect->getDiffuseColorProperty(), Vector3(0.20F, 0.35F, 0.50F));
                EXPECT_FLOAT_EQ(effect->getAlphaProperty(), 0.70F);
                EXPECT_FLOAT_EQ(effect->getEnvironmentMapAmountProperty(), 0.65F);
                EXPECT_EQ(effect->getEnvironmentMapSpecularProperty(), Vector3(0.80F, 0.60F, 0.40F));
                EXPECT_FLOAT_EQ(effect->getFresnelFactorProperty(), 3.0F);
                EXPECT_TRUE(effect->getFogEnabledProperty());
                EXPECT_EQ(effect->getFogColorProperty(), Vector3(0.08F, 0.12F, 0.16F));
                EXPECT_FLOAT_EQ(effect->getFogStartProperty(), 11.0F);
                EXPECT_FLOAT_EQ(effect->getFogEndProperty(), 71.0F);

                draw.fog = nullptr;
                ASSERT_TRUE(binder.BindEnvironmentMap(id, draw, environment).HasValue());
                EXPECT_FALSE(effect->getFogEnabledProperty());
                EXPECT_EQ(binder.EffectsCreated(), 1U);
            });
    }

    TEST(MaterialBinderTests, EnvironmentMapPassRefusesIncompleteOrInvalidInputsBeforeAllocation)
    {
        RunWithDeviceBinder(
            [](Gfx::GraphicsDevice& device, MaterialBinder& binder)
            {
                const Id id = Id::Of("MAT_CHROME");
                ASSERT_TRUE(binder.Register(id, MaterialDesc{}).HasValue());
                Gfx::Texture2D albedo(device, 2, 2);
                Gfx::TextureCube cube(device, 4, false, Gfx::SurfaceFormat::Color);
                DrawParams draw;
                EnvironmentMapParams environment;

                EXPECT_EQ(binder.BindEnvironmentMap(id, draw, environment).Error().Code(),
                          ErrorCode::InvalidArgument);
                draw.diffuse = &albedo;
                EXPECT_EQ(binder.BindEnvironmentMap(id, draw, environment).Error().Code(),
                          ErrorCode::InvalidArgument);

                environment.cubeMap = &cube;
                environment.amount = 1.01F;
                EXPECT_EQ(binder.BindEnvironmentMap(id, draw, environment).Error().Code(),
                          ErrorCode::OutOfRange);
                environment.amount = 1.0F;
                environment.fresnelFactor = -0.01F;
                EXPECT_EQ(binder.BindEnvironmentMap(id, draw, environment).Error().Code(),
                          ErrorCode::OutOfRange);
                EXPECT_EQ(binder.EffectsCreated(), 0U);

                EXPECT_EQ(binder.BindEnvironmentMap(Id::Of("MAT_UNKNOWN"), draw, environment).Error().Code(),
                          ErrorCode::NotFound);
            });
    }

    TEST(MaterialBinderTests, EnvironmentMapPassRefusesAnUnlitMaterial)
    {
        RunWithBinder(
            [](MaterialBinder& binder)
            {
                MaterialDesc desc;
                desc.lightingEnabled = false;
                const Id id = Id::Of("MAT_EMISSIVE_SCREEN");
                ASSERT_TRUE(binder.Register(id, desc).HasValue());

                const auto bound = binder.BindEnvironmentMap(id, DrawParams{}, EnvironmentMapParams{});
                ASSERT_FALSE(bound.HasValue());
                EXPECT_EQ(bound.Error().Code(), ErrorCode::Unsupported);
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
        RunWithDeviceBinder(
            [](Gfx::GraphicsDevice& device, MaterialBinder& binder)
            {
                ASSERT_TRUE(binder.Register(Id::Of("MAT_PET"), Pet()).HasValue());
                Gfx::Texture2D texture(device, 2, 2);

                DrawParams noBones;
                noBones.diffuse = &texture;
                EXPECT_EQ(binder.Bind(Id::Of("MAT_PET"), noBones).Error().Code(), ErrorCode::InvalidArgument);

                std::vector<Microsoft::Xna::Framework::Matrix> emptyPalette;
                DrawParams empty;
                empty.diffuse = &texture;
                empty.bones = &emptyPalette;
                EXPECT_EQ(binder.Bind(Id::Of("MAT_PET"), empty).Error().Code(), ErrorCode::InvalidArgument);

                std::vector<Microsoft::Xna::Framework::Matrix> oneBone(
                    1, Microsoft::Xna::Framework::Matrix::getIdentityProperty());
                DrawParams noTexture;
                noTexture.bones = &oneBone;
                EXPECT_EQ(binder.Bind(Id::Of("MAT_PET"), noTexture).Error().Code(),
                          ErrorCode::InvalidArgument);
                EXPECT_EQ(binder.EffectsCreated(), 0U)
                    << "invalid skinned draws must fail before allocating the shared effect";

                std::vector<Microsoft::Xna::Framework::Matrix> palette(
                    MaterialBinder::kMaxBones, Microsoft::Xna::Framework::Matrix::getIdentityProperty());
                DrawParams exact;
                exact.diffuse = &texture;
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
        RunWithDeviceBinder(
            [](Gfx::GraphicsDevice& device, MaterialBinder& binder)
            {
                MaterialDesc desc = Pet();
                desc.diffuse[0] = 0.30F;
                desc.diffuse[1] = 0.50F;
                desc.diffuse[2] = 0.70F;
                desc.alpha = 0.80F;
                desc.specularColour[0] = 0.10F;
                desc.specularColour[1] = 0.20F;
                desc.specularColour[2] = 0.25F;
                desc.specularPower = 19.0F;
                desc.perPixelLighting = true;
                const Id id = Id::Of("MAT_PET");
                ASSERT_TRUE(binder.Register(id, desc).HasValue());

                const auto firstBone = Microsoft::Xna::Framework::Matrix::CreateTranslation(
                    Microsoft::Xna::Framework::Vector3(1.0F, 2.0F, 3.0F));
                std::vector<Microsoft::Xna::Framework::Matrix> palette(
                    4, Microsoft::Xna::Framework::Matrix::getIdentityProperty());
                palette[0] = firstBone;
                Gfx::Texture2D texture(device, 2, 2);
                FogParams fog;
                fog.colour[0] = 0.10F;
                fog.colour[1] = 0.15F;
                fog.colour[2] = 0.20F;
                fog.start = 9.0F;
                fog.end = 63.0F;
                DrawParams draw;
                draw.diffuse = &texture;
                draw.bones = &palette;
                draw.fog = &fog;
                draw.directionalLights[0] = cnahouse::rendering::StockDirectionalLight{
                    Vector3(0.0F, -1.0F, 0.0F), Vector3(0.40F, 0.30F, 0.20F), Vector3()};
                draw.directionalLights[1] = cnahouse::rendering::StockDirectionalLight{
                    Vector3(1.0F, 0.0F, 0.0F), Vector3(0.10F, 0.12F, 0.14F), Vector3()};
                draw.directionalLights[2] = cnahouse::rendering::StockDirectionalLight{
                    Vector3(0.0F, 1.0F, 0.0F), Vector3(0.04F, 0.03F, 0.02F), Vector3()};

                const auto bound = binder.Bind(id, draw);
                ASSERT_TRUE(bound.HasValue()) << bound.Error().ToString();
                auto* effect = static_cast<Gfx::SkinnedEffect*>(*bound);
                EXPECT_EQ(effect->getTextureProperty(), &texture);
                EXPECT_EQ(effect->getDiffuseColorProperty(), Vector3(0.30F, 0.50F, 0.70F));
                EXPECT_FLOAT_EQ(effect->getAlphaProperty(), 0.80F);
                EXPECT_EQ(effect->getSpecularColorProperty(), Vector3(0.10F, 0.20F, 0.25F));
                EXPECT_FLOAT_EQ(effect->getSpecularPowerProperty(), 19.0F);
                EXPECT_TRUE(effect->getPreferPerPixelLightingProperty());
                EXPECT_TRUE(effect->getDirectionalLight0Property().getEnabledProperty());
                EXPECT_EQ(effect->getDirectionalLight0Property().getDiffuseColorProperty(),
                          Vector3(0.40F, 0.30F, 0.20F));
                EXPECT_TRUE(effect->getDirectionalLight1Property().getEnabledProperty());
                EXPECT_EQ(effect->getDirectionalLight1Property().getDirectionProperty(),
                          Vector3(1.0F, 0.0F, 0.0F));
                EXPECT_TRUE(effect->getDirectionalLight2Property().getEnabledProperty());
                EXPECT_EQ(effect->getWeightsPerVertexProperty(), 4);
                EXPECT_TRUE(effect->getFogEnabledProperty());
                EXPECT_EQ(effect->getFogColorProperty(), Vector3(0.10F, 0.15F, 0.20F));
                EXPECT_FLOAT_EQ(effect->getFogStartProperty(), 9.0F);
                EXPECT_FLOAT_EQ(effect->getFogEndProperty(), 63.0F);

                const auto applied = effect->GetBoneTransforms(static_cast<int>(MaterialBinder::kMaxBones));
                ASSERT_EQ(applied.size(), MaterialBinder::kMaxBones);
                EXPECT_EQ(applied[0], firstBone);
                EXPECT_EQ(applied[3], Microsoft::Xna::Framework::Matrix::getIdentityProperty());
                EXPECT_EQ(applied[4], Microsoft::Xna::Framework::Matrix::getIdentityProperty())
                    << "the first slot beyond the short skin must be identity padding";
                EXPECT_EQ(applied.back(), Microsoft::Xna::Framework::Matrix::getIdentityProperty());

                draw.fog = nullptr;
                ASSERT_TRUE(binder.Bind(id, draw).HasValue());
                EXPECT_FALSE(effect->getFogEnabledProperty());
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
