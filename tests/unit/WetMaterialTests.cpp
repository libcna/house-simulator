// SPDX-License-Identifier: MIT
//
// `HOUSE-01748`: Tier S selects the fourteen authored wet endpoints from integrated wetness.
#include <array>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/rendering/StaticGeometryPass.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::rendering::FindTierSWetVariant;
    using cnahouse::rendering::SelectTierSMaterial;
    using cnahouse::util::Id;

    constexpr std::array<const char*, 14> kDryMaterials = {
        "MAT_ASPHALT_01",
        "MAT_BLUESTONE_PAVER",
        "MAT_BRICK_WATER_TABLE",
        "MAT_CONCRETE_BROOM",
        "MAT_CONCRETE_KERB",
        "MAT_CONCRETE_SLAB",
        "MAT_DECK_WOOD",
        "MAT_GRAVEL_PATH",
        "MAT_GROUND_LAWN",
        "MAT_ROOF_SHINGLE",
        "MAT_SIDING_DUSTY_BLUE",
        "MAT_SIDING_SAGE",
        "MAT_SIDING_WARM_WHITE",
        "MAT_SOIL_GARDEN",
    };
} // namespace

TEST(WetMaterialTests, AllFourteenAuthoredPairsResolveAndNoExcludedFinishDoes)
{
    cnahouse::world::WorldData::Contents contents;
    const std::string root = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world";
    ASSERT_TRUE(cnahouse::world::WorldLoader::LoadMaterials(root, contents));

    for (const char* name : kDryMaterials)
    {
        const Id wet = FindTierSWetVariant(contents.materials, name);
        EXPECT_EQ(wet, Id::Of(std::string(name) + "_WET")) << name;
    }
    EXPECT_FALSE(FindTierSWetVariant(contents.materials, "MAT_SOFFIT_WHITE").IsValid());
    EXPECT_FALSE(FindTierSWetVariant(contents.materials, "MAT_METAL_GUTTER").IsValid());
    EXPECT_FALSE(FindTierSWetVariant(contents.materials, "MAT_BALCONY_METAL").IsValid());
}

TEST(WetMaterialTests, TheEndpointSwitchesAtHalfAndReturnsAsWetnessFalls)
{
    const Id dry = Id::Of("MAT_ASPHALT_01");
    const Id wet = Id::Of("MAT_ASPHALT_01_WET");

    EXPECT_EQ(SelectTierSMaterial(dry, wet, 0.0F), dry);
    EXPECT_EQ(SelectTierSMaterial(dry, wet, 0.499F), dry);
    EXPECT_EQ(SelectTierSMaterial(dry, wet, 0.5F), wet);
    EXPECT_EQ(SelectTierSMaterial(dry, wet, 1.0F), wet);
    EXPECT_EQ(SelectTierSMaterial(dry, wet, 0.2F), dry);
    EXPECT_EQ(SelectTierSMaterial(dry, {}, 1.0F), dry);
    EXPECT_EQ(SelectTierSMaterial(dry, wet, std::numeric_limits<float>::quiet_NaN()), dry);
}

TEST(WetMaterialTests, UnbakedDrivewayUsesTheCanonicalConcreteWetEndpoint)
{
    cnahouse::world::WorldData::Contents contents;
    const std::string root = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world";
    ASSERT_TRUE(cnahouse::world::WorldLoader::LoadMaterials(root, contents));

    EXPECT_EQ(FindTierSWetVariant(contents.materials, "MAT_OUTDOOR_CONCRETE"),
              Id::Of("MAT_CONCRETE_BROOM_WET"));
}
