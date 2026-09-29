// SPDX-License-Identifier: MIT
//
// `HOUSE-01265`. Section 28.4's two-hop light flood is graph arithmetic, so its exact range,
// aperture response and source cap are pinned on a four-cell chain. The authored-world acceptance
// lives beside `LightingSystem`: this fixture proves the algorithm rather than restating that data.
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/lighting/BorrowedLightModel.hpp"
#include "cnahouse/lighting/RoomLightState.hpp"
#include "cnahouse/visibility/PortalRuntime.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace
{
    using cnahouse::lighting::BorrowedLightModel;
    using cnahouse::lighting::RoomLightState;
    using cnahouse::util::Id;
    using cnahouse::visibility::PortalRuntime;
    namespace world = cnahouse::world;

    world::Cell Cell(std::string_view name, float offset)
    {
        world::Cell cell;
        cell.id = Id::Of(name);
        cell.name = std::string(name);
        cell.boxes.push_back(world::Footprint{offset, offset + 1.0F, 0.0F, 1.0F});
        return cell;
    }

    world::Portal Portal(std::string_view name, Id a, Id b, float width = 1.0F)
    {
        world::Portal portal;
        portal.id = Id::Of(name);
        portal.cellA = a;
        portal.cellB = b;
        portal.minU = 0.0F;
        portal.maxU = width;
        portal.minV = 0.0F;
        portal.maxV = 1.0F;
        portal.kind = world::PortalKind::Door;
        portal.aperture = Id::Of("LEAF");
        portal.opacity = world::PortalOpacity::OpaqueWhenClosed;
        return portal;
    }

    world::WorldData Chain(float firstPortalWidth = 1.0F)
    {
        world::WorldData::Contents contents;
        contents.cells.push_back(Cell("A", 0.0F));
        contents.cells.push_back(Cell("B", 1.0F));
        contents.cells.push_back(Cell("C", 2.0F));
        contents.cells.push_back(Cell("D", 3.0F));
        contents.portals.push_back(
            Portal("P_AB", contents.cells[0].id, contents.cells[1].id, firstPortalWidth));
        contents.portals.push_back(Portal("P_BC", contents.cells[1].id, contents.cells[2].id));
        contents.portals.push_back(Portal("P_CD", contents.cells[2].id, contents.cells[3].id));
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << built.Error().ToString();
        return std::move(built.Value());
    }

    std::vector<PortalRuntime> Runtimes(const world::WorldData& data)
    {
        std::vector<PortalRuntime> runtimes;
        runtimes.reserve(data.Portals().size());
        for (const world::Portal& portal : data.Portals())
        {
            runtimes.emplace_back(portal);
        }
        return runtimes;
    }

    std::vector<RoomLightState> SourceInA(const world::WorldData& data)
    {
        std::vector<RoomLightState> states(data.Cells().size());
        for (std::size_t index = 0; index < states.size(); ++index)
        {
            states[index].cell = data.Cells()[index].id;
        }
        states.at(0).artificial = 1.0F;
        return states;
    }

} // namespace

TEST(BorrowedLightModelTests, ClosedDoorsPassNothingAndOpeningIsContinuousWithAperture)
{
    const world::WorldData data = Chain();
    std::vector<PortalRuntime> portals = Runtimes(data);
    BorrowedLightModel model(data, portals);
    const std::vector<RoomLightState> states = SourceInA(data);
    std::vector<float> borrowed(states.size());

    model.Evaluate(states, borrowed);
    EXPECT_EQ(borrowed, (std::vector<float>{0.0F, 0.0F, 0.0F, 0.0F}));

    EXPECT_TRUE(portals[0].SetAperture(0.5F));
    EXPECT_TRUE(portals[1].SetAperture(1.0F));
    EXPECT_TRUE(portals[2].SetAperture(1.0F));
    model.Evaluate(states, borrowed);
    EXPECT_NEAR(borrowed[0], 0.0F, 1e-6F);
    EXPECT_NEAR(borrowed[1], 0.15F, 1e-6F);
    EXPECT_NEAR(borrowed[2], 0.045F, 1e-6F);
    EXPECT_NEAR(borrowed[3], 0.0F, 1e-6F) << "a third hop escaped the fixed-radius flood";
}

TEST(BorrowedLightModelTests, EachOriginIsCappedAndLightDoesNotBounceBackIntoIt)
{
    const world::WorldData data = Chain(2.0F);
    std::vector<PortalRuntime> portals = Runtimes(data);
    for (PortalRuntime& portal : portals)
    {
        EXPECT_TRUE(portal.SetAperture(1.0F));
    }
    BorrowedLightModel model(data, portals);
    const std::vector<RoomLightState> states = SourceInA(data);
    std::vector<float> borrowed(states.size());

    model.Evaluate(states, borrowed);
    EXPECT_NEAR(borrowed[0], 0.0F, 1e-6F) << "A -> B -> A is not useful borrowed light";
    EXPECT_NEAR(borrowed[1], 0.35F, 1e-6F) << "the 0.60 raw transfer escaped the source cap";
    EXPECT_NEAR(borrowed[2], 0.105F, 1e-6F);
    EXPECT_NEAR(borrowed[3], 0.0F, 1e-6F);
}

TEST(BorrowedLightModelTests, AmbientAndBorrowedInputsCannotManufactureANewSource)
{
    const world::WorldData data = Chain();
    std::vector<PortalRuntime> portals = Runtimes(data);
    for (PortalRuntime& portal : portals)
    {
        EXPECT_TRUE(portal.SetAperture(1.0F));
    }
    BorrowedLightModel model(data, portals);
    std::vector<RoomLightState> states(data.Cells().size());
    states.at(0).borrowed = 1.0F;
    std::vector<float> borrowed(states.size(), 1.0F);

    model.Evaluate(states, borrowed);
    EXPECT_EQ(borrowed, (std::vector<float>{0.0F, 0.0F, 0.0F, 0.0F}));
}

TEST(BorrowedLightModelTests, DaylightTransferDoesNotTreatAnArtificialLampAsSky)
{
    const world::WorldData data = Chain();
    std::vector<PortalRuntime> portals = Runtimes(data);
    for (PortalRuntime& portal : portals)
    {
        EXPECT_TRUE(portal.SetAperture(1.0F));
    }
    BorrowedLightModel model(data, portals);
    std::vector<RoomLightState> states(data.Cells().size());
    states.at(0).daylight = 1.0F;
    states.at(3).artificial = 1.0F;
    std::vector<float> borrowedDaylight(states.size());

    model.EvaluateDaylight(states, borrowedDaylight);
    EXPECT_NEAR(borrowedDaylight[0], 0.0F, 1e-6F);
    EXPECT_NEAR(borrowedDaylight[1], 0.30F, 1e-6F);
    EXPECT_NEAR(borrowedDaylight[2], 0.09F, 1e-6F);
    EXPECT_NEAR(borrowedDaylight[3], 0.0F, 1e-6F);
}
