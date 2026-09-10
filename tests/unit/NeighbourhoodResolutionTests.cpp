// SPDX-License-Identifier: MIT
//
// `HOUSE-00856`'s acceptance, against the world the build actually produced: every `neighbourhood`
// row in `layout.exterior.json` resolves to a mesh in `content/world/neighbourhood.bin`.
//
// This is the check that could not exist before the task. `neighbourhood_gen.py` had written
// `build/neighbourhood/<ASSET>.glb` since `HOUSE-00841` and NOTHING read that directory: fifteen
// tasks of houses, impostor cards, street furniture and a horizon, none of which any code could
// open. A green suite over a neighbourhood nobody can load is exactly the failure the project's
// own `HOUSE-00785` and `HOUSE-00227` were about, one subsystem further out.
//
// Skipped, loudly, on a checkout that has not run the content build -- the binary is generated and
// is not committed.
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/world/NeighbourhoodReader.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::world::NeighbourBuilding;
    using cnahouse::world::NeighbourhoodLibrary;
    using cnahouse::world::NeighbourhoodReader;

    namespace world = cnahouse::world;

    constexpr const char* kBinary = "content/world/neighbourhood.bin";

    bool ContentIsBuilt()
    {
        return std::filesystem::exists(kBinary) &&
               std::filesystem::exists("content/world/layout.exterior.json");
    }

    std::vector<NeighbourBuilding> Rows()
    {
        world::WorldData::Contents contents;
        EXPECT_TRUE(world::WorldLoader::LoadExterior("content/world", contents).HasValue());
        return contents.exterior.neighbourhood;
    }

    NeighbourhoodLibrary Load()
    {
        auto library = NeighbourhoodReader::ReadFromTitle(kBinary);
        EXPECT_TRUE(library) << (library ? std::string() : library.Error().ToString());
        return library ? std::move(*library) : NeighbourhoodLibrary{};
    }

} // namespace

TEST(NeighbourhoodResolutionTests, TheDeployedBinaryReads)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << kBinary << " is not built; run tools/ci/build_content.py --only world";
    }
    const NeighbourhoodLibrary library = Load();
    // §11.4's own count, which is what makes this a claim rather than a smoke test: nineteen house
    // variants, four impostor cards, seven pieces of street furniture, three horizon cards and the
    // water tower.
    EXPECT_EQ(library.assets.size(), 34u);
    EXPECT_FALSE(library.worldHash.empty())
        << "the binary carries no world hash, so nothing downstream can tell it is stale";
    for (const auto& asset : library.assets)
    {
        EXPECT_FALSE(asset.primitives.empty()) << asset.asset << " draws nothing";
    }
}

TEST(NeighbourhoodResolutionTests, EveryRowExceptTheVehiclesResolvesToAMesh)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << kBinary << " is not built; run tools/ci/build_content.py --only world";
    }
    const NeighbourhoodLibrary library = Load();
    const std::vector<NeighbourBuilding> rows = Rows();
    ASSERT_FALSE(rows.empty());

    const std::vector<std::string> missing = UnresolvedAssets(library, rows);
    // §11.4's vehicles are `HOUSE-00847`'s to deliver and share this array. They are named here
    // rather than filtered out, so the day that task lands this list becomes empty and the test
    // says so instead of quietly continuing to accept a gap.
    const std::vector<std::string> expected{"MODEL_DELIVERY_VAN", "MODEL_PARKED_CAR"};
    EXPECT_EQ(missing, expected) << "a row naming a mesh nobody drew is a house that is simply not there";
}

TEST(NeighbourhoodResolutionTests, EveryMeshTheBinaryHoldsIsActuallyPlaced)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << kBinary << " is not built; run tools/ci/build_content.py --only world";
    }
    // The other direction, which ADR-0013's first rule asks for: only the combinations the layout
    // names are generated, so a mesh no row names is a file shipped in a pack budget for nothing.
    const NeighbourhoodLibrary library = Load();
    const std::vector<NeighbourBuilding> rows = Rows();
    std::vector<std::string> unplaced;
    for (const auto& asset : library.assets)
    {
        const cnahouse::util::Id id = cnahouse::util::Id::Of(asset.asset);
        const bool placed = std::any_of(
            rows.begin(), rows.end(), [id](const NeighbourBuilding& row) { return row.asset == id; });
        if (!placed)
        {
            unplaced.push_back(asset.asset);
        }
    }
    EXPECT_TRUE(unplaced.empty()) << unplaced.size() << " mesh(es) nothing places, the first being "
                                  << (unplaced.empty() ? std::string() : unplaced.front());
}
