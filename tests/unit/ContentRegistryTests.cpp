// SPDX-License-Identifier: MIT
//
// `HOUSE-00142`.
#include <gtest/gtest.h>

#include "cnahouse/content/ContentRegistry.hpp"
#include "cnahouse/util/Ids.hpp"

namespace
{
    using cnahouse::content::AssetKind;
    using cnahouse::content::ContentRegistry;
    using cnahouse::util::ErrorCode;

    constexpr std::string_view kManifest = R"({
  "schema": "cna-house/assets/1",
  "assets": [
    { "id": "MODEL_PROP_KITCHEN_FRIDGE_01", "kind": "model",
      "contentName": "Models/Kitchen/fridge_01", "residencyPack": "house-l0" },
    { "id": "TEX_MAT_APPLIANCE_STEEL", "kind": "texture",
      "contentName": "Textures/Materials/appliance_steel", "residencyPack": "house-l0" },
    { "id": "SND_FRIDGE_HUM", "kind": "sound",
      "contentName": "Audio/Appliances/fridge_hum", "residencyPack": "audio-ambience" },
    { "id": "FONT_HUD", "kind": "font", "contentName": "Fonts/Hud" }
  ]
})";

    class ContentRegistryTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            cnahouse::util::IdRegistry::ResetForTesting();
        }

        void TearDown() override
        {
            cnahouse::util::IdRegistry::ResetForTesting();
        }
    };

    TEST_F(ContentRegistryTest, LoadsEveryRow)
    {
        ContentRegistry registry;
        auto loaded = registry.LoadFromJson(kManifest, "assets.manifest.json");
        ASSERT_TRUE(loaded) << loaded.Error().ToString();
        EXPECT_EQ(registry.Count(), 4u);
    }

    TEST_F(ContentRegistryTest, LooksUpByNameAndById)
    {
        ContentRegistry registry;
        ASSERT_TRUE(registry.LoadFromJson(kManifest, "assets.manifest.json"));

        const auto* byName = registry.Find("MODEL_PROP_KITCHEN_FRIDGE_01");
        ASSERT_NE(byName, nullptr);
        EXPECT_EQ(byName->contentName, "Models/Kitchen/fridge_01");
        EXPECT_EQ(byName->kind, AssetKind::Model);

        // World data references props by id, not by path, so renaming a file is not a world-data
        // migration. The registry is what turns one into the other.
        const auto* byId = registry.Find(byName->id);
        EXPECT_EQ(byId, byName);
    }

    TEST_F(ContentRegistryTest, ResidencyPackDefaultsToCore)
    {
        // An asset with no pack is one the game always needs -- the HUD font is the obvious case --
        // and defaulting it to `core` is what makes that the quiet path rather than an authoring
        // obligation on every row.
        ContentRegistry registry;
        ASSERT_TRUE(registry.LoadFromJson(kManifest, "assets.manifest.json"));
        const auto* font = registry.Find("FONT_HUD");
        ASSERT_NE(font, nullptr);
        EXPECT_EQ(font->pack, "core");
    }

    TEST_F(ContentRegistryTest, AssetsAreGroupedByPackForTheResidencySystem)
    {
        // Streaming promotes and evicts by pack, and nothing in a content name says which pack an
        // asset is in. This mapping is the whole reason the registry exists.
        ContentRegistry registry;
        ASSERT_TRUE(registry.LoadFromJson(kManifest, "assets.manifest.json"));
        EXPECT_EQ(registry.InPack("house-l0").size(), 2u);
        EXPECT_EQ(registry.InPack("audio-ambience").size(), 1u);
        EXPECT_TRUE(registry.InPack("nonexistent").empty());

        const auto packs = registry.Packs();
        EXPECT_EQ(packs.size(), 3u);
        EXPECT_EQ(packs.front(), "house-l0") << "first-seen order, so the list is stable across runs";
    }

    TEST_F(ContentRegistryTest, AWrongSchemaVersionIsItsOwnError)
    {
        // Distinguishable from "corrupt", because a future migration needs to tell them apart.
        ContentRegistry registry;
        auto loaded = registry.LoadFromJson(R"({"schema": "cna-house/assets/2", "assets": []})",
                                            "assets.manifest.json");
        ASSERT_FALSE(loaded);
        EXPECT_EQ(loaded.Error().Code(), ErrorCode::VersionMismatch);
    }

    TEST_F(ContentRegistryTest, EveryBadRowIsReportedNotJustTheFirst)
    {
        // `docs/conventions.md` §5.1's accumulation rule. Fixing forty authoring mistakes one build at
        // a time is intolerable, and a manifest is exactly the file that accumulates them.
        constexpr std::string_view kBad = R"({
      "schema": "cna-house/assets/1",
      "assets": [
        { "kind": "model", "contentName": "a" },
        { "id": "B", "contentName": "b" },
        { "id": "C", "kind": "nonsense", "contentName": "c" },
        { "id": "D", "kind": "model" }
      ]
    })";
        ContentRegistry registry;
        auto loaded = registry.LoadFromJson(kBad, "assets.manifest.json");
        ASSERT_FALSE(loaded);
        const std::string message = loaded.Error().Message();
        EXPECT_NE(message.find("4 row(s)"), std::string::npos) << message;
        EXPECT_NE(message.find("nonsense"), std::string::npos)
            << "and each problem names itself: " << message;
    }

    TEST_F(ContentRegistryTest, ADuplicateIdIsRejected)
    {
        // Two assets under one id would silently become one asset.
        constexpr std::string_view kDuplicate = R"({
      "schema": "cna-house/assets/1",
      "assets": [
        { "id": "SAME", "kind": "model", "contentName": "a" },
        { "id": "SAME", "kind": "model", "contentName": "b" }
      ]
    })";
        ContentRegistry registry;
        auto loaded = registry.LoadFromJson(kDuplicate, "assets.manifest.json");
        ASSERT_FALSE(loaded);
        EXPECT_NE(loaded.Error().Message().find("appears twice"), std::string::npos);
    }

    TEST_F(ContentRegistryTest, AFailedLoadLeavesTheRegistryEmptyRatherThanHalfPopulated)
    {
        // A half-loaded manifest is worse than none: the game would start and then fail at the first
        // asset that happened to be in the missing half.
        ContentRegistry registry;
        ASSERT_TRUE(registry.LoadFromJson(kManifest, "assets.manifest.json"));
        ASSERT_EQ(registry.Count(), 4u);

        auto reload = registry.LoadFromJson(R"({"schema": "cna-house/assets/1",
        "assets": [{ "id": "X", "kind": "model" }]})",
                                            "assets.manifest.json");
        ASSERT_FALSE(reload);
        EXPECT_EQ(registry.Count(), 0u);
    }

    TEST_F(ContentRegistryTest, AnEmptyManifestIsValid)
    {
        // A build with no content yet is a legitimate state, and it is the state a fresh clone is in.
        ContentRegistry registry;
        auto loaded = registry.LoadFromJson(R"({"schema": "cna-house/assets/1", "assets": []})",
                                            "assets.manifest.json");
        EXPECT_TRUE(loaded) << (loaded ? "" : loaded.Error().ToString());
        EXPECT_EQ(registry.Count(), 0u);
    }

} // namespace
