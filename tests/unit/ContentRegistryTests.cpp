// SPDX-License-Identifier: MIT
//
// `HOUSE-00142`.
#include <fstream>
#include <sstream>

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
    { "id": "FONT_UI_16", "kind": "font", "contentName": "Fonts/ui-16",
      "residencyPack": "core" },
    { "id": "FONT_NOTO_SANS_REGULAR",
      "notPackaged": "a typeface the .spritefont descriptors rasterise; not a runtime asset" }
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
        // FIVE rows in, FOUR assets out: the fifth declares `notPackaged` and is a build input.
        EXPECT_EQ(registry.Count(), 4u);
        EXPECT_EQ(registry.Find("FONT_NOTO_SANS_REGULAR"), nullptr);
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

    TEST_F(ContentRegistryTest, AResidencyPackIsRequiredRatherThanDefaultedToCore)
    {
        // CHANGED by `HOUSE-00202`, and the old behaviour is worth recording. `residencyPack` used
        // to default to `core`, so a row that named no pack became an always-resident one: the
        // single most expensive pack to be wrong about, since the asset would be pinned for the
        // whole session and appear in no download budget. `manifest.py` now refuses such a row, so
        // requiring it here costs an author nothing and closes the gap between what the gate
        // enforces and what the runtime accepts.
        constexpr std::string_view kNoPack = R"({
  "schema": "cna-house/assets/1",
  "assets": [
    { "id": "FONT_UI_16", "kind": "font", "contentName": "Fonts/ui-16" }
  ]
})";
        ContentRegistry registry;
        auto loaded = registry.LoadFromJson(kNoPack, "assets.manifest.json");
        EXPECT_FALSE(loaded) << "a row with no residencyPack must be rejected, not defaulted";
        EXPECT_EQ(registry.Count(), 0u);

        ContentRegistry good;
        ASSERT_TRUE(good.LoadFromJson(kManifest, "assets.manifest.json"));
        const auto* font = good.Find("FONT_UI_16");
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

    TEST_F(ContentRegistryTest, TheREALManifestLoads)
    {
        // `HOUSE-00202`. Until this test existed, the runtime registry read a schema that nothing
        // produced: `assets.manifest.json` carried provenance -- id, category, sourceFile, hashes,
        // origin -- and none of `contentName`, `kind` or `residencyPack`. Every unit test above
        // passes against a hand-written fixture, which is exactly how that gap survived.
        //
        // The test working directory is the repository root (`tests/CMakeLists.txt`), so this path
        // is stable wherever the build tree lives.
        std::ifstream file("assets-src/assets.manifest.json");
        ASSERT_TRUE(file.is_open()) << "assets-src/assets.manifest.json is not readable from the "
                                       "repository root";
        std::ostringstream text;
        text << file.rdbuf();

        ContentRegistry registry;
        auto loaded = registry.LoadFromJson(text.str(), "assets-src/assets.manifest.json");
        ASSERT_TRUE(loaded) << loaded.Error().ToString();
        EXPECT_GT(registry.Count(), 0u);

        // Every packaged asset names a pack, and every pack it names is one the residency system
        // knows about. `manifest.py validate` checks the same thing offline; this checks that the
        // RUNTIME agrees, which is the half that was missing.
        for (const auto& entry : registry.All())
        {
            EXPECT_FALSE(entry.pack.empty()) << entry.name << " has no residency pack";
            EXPECT_FALSE(entry.contentName.empty()) << entry.name << " has no content name";
            EXPECT_NE(entry.kind, cnahouse::content::AssetKind::Unknown) << entry.name;
        }
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
        { "id": "SAME", "kind": "model", "contentName": "a", "residencyPack": "core" },
        { "id": "SAME", "kind": "model", "contentName": "b", "residencyPack": "core" }
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
