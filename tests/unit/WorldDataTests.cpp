// SPDX-License-Identifier: MIT
//
// `HOUSE-00342`. The immutable world model: its indices, its per-cell groupings, and the two
// invariants `Create` is the only place that can enforce.
//
// The fixture is the same two-storey house `tools/world/validate_world.py` validates -- a foyer, a
// hall, a stacked pair of WCs, a stair through a horizontal `stair_well` portal, a terrace and a
// closet nobody can stand in. Building it here in C++ rather than loading the Python fixture's
// JSON is deliberate: `WorldLoader` does not exist yet (`HOUSE-00343`), and a model test that
// needed a loader would be testing the loader.
#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace
{
    using cnahouse::util::ErrorCode;
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;
    using cnahouse::util::Intern;
    using Vector3 = Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    world::Footprint Box(float minX, float maxX, float minZ, float maxZ)
    {
        return world::Footprint{minX, maxX, minZ, maxZ};
    }

    world::Cell MakeCell(std::string_view id,
                         std::string_view level,
                         world::CellKind kind,
                         std::vector<world::Footprint> boxes)
    {
        world::Cell cell;
        cell.id = Intern(id);
        cell.level = Intern(level);
        cell.kind = kind;
        cell.boxes = std::move(boxes);
        return cell;
    }

    world::Portal MakePortal(std::string_view id,
                             std::string_view a,
                             std::string_view b,
                             world::PlaneAxis axis,
                             float planeValue,
                             float minU,
                             float maxU,
                             float minV,
                             float maxV,
                             world::PortalKind kind)
    {
        world::Portal portal;
        portal.id = Intern(id);
        portal.cellA = Intern(a);
        portal.cellB = Intern(b);
        portal.axis = axis;
        portal.planeValue = planeValue;
        portal.minU = minU;
        portal.maxU = maxU;
        portal.minV = minV;
        portal.maxV = maxV;
        portal.kind = kind;
        return portal;
    }

    world::WorldData::Contents Fixture()
    {
        world::WorldData::Contents contents;

        world::Level l0;
        l0.id = Intern("L0");
        l0.name = "Main Floor";
        l0.ffl = 0.60F;
        l0.ceiling = 3.30F;
        l0.structureDepth = 0.35F;

        world::Level l1;
        l1.id = Intern("L1");
        l1.name = "Upper Floor";
        l1.ffl = 3.65F;
        l1.ceiling = 6.20F;
        l1.structureDepth = 0.35F;

        // The attic: `ceiling` is absent because rafters bound it, not a plane. A cell here without
        // a `yOverride` has no extent, which is the case `ExtentOf` must refuse rather than invent.
        world::Level l3;
        l3.id = Intern("L3");
        l3.name = "Attic";
        l3.ffl = 9.30F;
        l3.roof = Intern("ROOF_MAIN");
        contents.levels = {l0, l1, l3};

        contents.construction.wallExterior = 0.30F;
        contents.construction.wallPartition = 0.15F;

        contents.cells = {
            MakeCell("L0_FOYER", "L0", world::CellKind::Room, {Box(-2.0F, 2.0F, 0.0F, 4.0F)}),
            MakeCell("L0_HALL", "L0", world::CellKind::Corridor, {Box(-2.0F, 2.0F, 4.0F, 10.0F)}),
            MakeCell("L0_WC1", "L0", world::CellKind::Room, {Box(2.0F, 4.0F, 4.0F, 6.0F)}),
            MakeCell("L0_TERRACE", "L0", world::CellKind::Exterior, {Box(-2.0F, 2.0F, -4.0F, 0.0F)}),
            MakeCell("L1_WC4", "L1", world::CellKind::Room, {Box(2.0F, 4.0F, 4.0F, 6.0F)}),
            MakeCell("L3_STORE", "L3", world::CellKind::Closet, {Box(-2.0F, 2.0F, 4.0F, 6.0F)}),
        };
        // An L-shaped room: two boxes, and the notch between them is outside the cell. This is the
        // shape `CellContains` has to get right and a single bounding box cannot.
        contents.cells[1].boxes.push_back(Box(2.0F, 6.0F, 8.0F, 10.0F));

        contents.portals = {
            MakePortal("P_FOYER__HALL",
                       "L0_FOYER",
                       "L0_HALL",
                       world::PlaneAxis::Z,
                       4.0F,
                       -0.5F,
                       0.5F,
                       0.60F,
                       2.65F,
                       world::PortalKind::CasedOpening),
            MakePortal("P_HALL__WC1",
                       "L0_HALL",
                       "L0_WC1",
                       world::PlaneAxis::X,
                       2.0F,
                       4.6F,
                       5.5F,
                       0.60F,
                       2.62F,
                       world::PortalKind::Door),
            MakePortal("P_FOYER__TERRACE",
                       "L0_FOYER",
                       "L0_TERRACE",
                       world::PlaneAxis::Z,
                       0.0F,
                       -0.5F,
                       0.5F,
                       0.60F,
                       2.65F,
                       world::PortalKind::ExteriorDoor),
            MakePortal("P_HALL__WINDOW",
                       "L0_HALL",
                       "L0_TERRACE",
                       world::PlaneAxis::X,
                       -2.0F,
                       5.0F,
                       6.2F,
                       1.00F,
                       2.20F,
                       world::PortalKind::Window),
        };

        world::Light light;
        light.id = Intern("LIGHT_HALL");
        light.cell = Intern("L0_HALL");
        light.group = Intern("LG_HALL");
        light.position = Vector3(0.0F, 3.10F, 7.0F);
        world::Light second = light;
        second.id = Intern("LIGHT_HALL_2");
        second.position = Vector3(0.0F, 3.10F, 5.0F);
        world::Light foyer = light;
        foyer.id = Intern("LIGHT_FOYER");
        foyer.cell = Intern("L0_FOYER");
        contents.lights = {light, foyer, second};

        world::Prop pan;
        pan.id = Intern("PROP_WC1_PAN");
        pan.asset = Intern("MODEL_WC");
        pan.cell = Intern("L0_WC1");
        pan.position = Vector3(3.0F, 0.60F, 5.0F);
        pan.plumbing = Intern("STACK_A");
        contents.props = {pan};

        world::PlumbingStack stack;
        stack.id = Intern("STACK_A");
        stack.cells = {Intern("L0_WC1"), Intern("L1_WC4")};
        stack.chase = Box(2.0F, 4.0F, 4.0F, 6.0F);
        contents.plumbing = {stack};

        world::Material tile;
        tile.id = Intern("MAT_TILE");
        tile.materialClass = world::MaterialClass::Tile;
        contents.materials = {tile};

        return contents;
    }

    class WorldDataTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            IdRegistry::ResetForTesting();
        }

        void TearDown() override
        {
            IdRegistry::ResetForTesting();
        }
    };

    // --- what `Create` is the only place that can check ---------------------------------------

    TEST_F(WorldDataTest, AFixtureHouseIndexesCleanly)
    {
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world) << world.Error().ToString();
        EXPECT_EQ(world.Value().Cells().size(), 6U);
        EXPECT_EQ(world.Value().Portals().size(), 4U);
        EXPECT_EQ(world.Value().Levels().size(), 3U);
    }

    TEST_F(WorldDataTest, ADuplicateIdIsRefusedAndBothRowsAreNamed)
    {
        auto contents = Fixture();
        contents.cells.push_back(
            MakeCell("L0_HALL", "L0", world::CellKind::Room, {Box(0.0F, 1.0F, 0.0F, 1.0F)}));

        auto world = world::WorldData::Create(std::move(contents));
        ASSERT_FALSE(world);
        EXPECT_EQ(world.Error().Code(), ErrorCode::Duplicate);
        EXPECT_NE(world.Error().Message().find("L0_HALL"), std::string::npos)
            << "the message must name the id, not its hash: " << world.Error().ToString();
        EXPECT_NE(world.Error().Context().find("cell["), std::string::npos)
            << "and the row it is in: " << world.Error().ToString();
    }

    TEST_F(WorldDataTest, IdsAreUniqueAcrossEveryKindAndNotWithinOne)
    {
        // A material called `L0_HALL` is a duplicate even though no other MATERIAL is called that.
        // References in the data are bare ids and do not carry the kind they point at, so two kinds
        // sharing a name make a resolved reference answer a question nobody asked.
        auto contents = Fixture();
        world::Material clash;
        clash.id = Intern("L0_HALL");
        clash.materialClass = world::MaterialClass::Paint;
        contents.materials.push_back(clash);

        auto world = world::WorldData::Create(std::move(contents));
        ASSERT_FALSE(world);
        EXPECT_EQ(world.Error().Code(), ErrorCode::Duplicate);
        EXPECT_NE(world.Error().Message().find("cell"), std::string::npos)
            << "the message must say which OTHER kind already used it: " << world.Error().ToString();
    }

    TEST_F(WorldDataTest, ARowWithNoIdIsRefused)
    {
        auto contents = Fixture();
        contents.cells.push_back(world::Cell{});

        auto world = world::WorldData::Create(std::move(contents));
        ASSERT_FALSE(world);
        EXPECT_EQ(world.Error().Code(), ErrorCode::InvalidData);
    }

    // --- lookup ---------------------------------------------------------------------------------

    TEST_F(WorldDataTest, LookupFindsARowAndReturnsNullForAnUnknownId)
    {
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);

        const world::Cell* hall = world.Value().FindCell(Intern("L0_HALL"));
        ASSERT_NE(hall, nullptr);
        EXPECT_EQ(hall->kind, world::CellKind::Corridor);

        // Null, not a default row. A silent empty cell would let the physics step carry on in a
        // room with no floor rather than clamping to the last good one (§16.4 step 4).
        EXPECT_EQ(world.Value().FindCell(Intern("L0_BALLROOM")), nullptr);
        EXPECT_EQ(world.Value().FindCell(Id{}), nullptr);
    }

    TEST_F(WorldDataTest, EveryKindIsIndexed)
    {
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);
        const world::WorldData& data = world.Value();

        EXPECT_NE(data.FindLevel(Intern("L1")), nullptr);
        EXPECT_NE(data.FindCell(Intern("L0_WC1")), nullptr);
        EXPECT_NE(data.FindPortal(Intern("P_HALL__WC1")), nullptr);
        EXPECT_NE(data.FindLight(Intern("LIGHT_FOYER")), nullptr);
        EXPECT_NE(data.FindMaterial(Intern("MAT_TILE")), nullptr);
        EXPECT_NE(data.FindProp(Intern("PROP_WC1_PAN")), nullptr);
        EXPECT_NE(data.FindPlumbingStack(Intern("STACK_A")), nullptr);
    }

    // --- the per-cell groupings -----------------------------------------------------------------

    TEST_F(WorldDataTest, APortalBelongsToBothOfItsCells)
    {
        // The claim that matters. A portal grouped only under `cellA` is invisible from the room on
        // the other side of it, and every caller -- visibility, nav, audio transmission -- asks
        // "what leads out of THIS cell".
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);
        const world::WorldData& data = world.Value();

        EXPECT_EQ(data.PortalsOf(Intern("L0_FOYER")).size(), 2U);
        EXPECT_EQ(data.PortalsOf(Intern("L0_HALL")).size(), 3U);
        EXPECT_EQ(data.PortalsOf(Intern("L0_WC1")).size(), 1U);
        EXPECT_EQ(data.PortalsOf(Intern("L0_TERRACE")).size(), 2U);

        const auto wc = data.PortalsOf(Intern("L0_WC1"));
        ASSERT_EQ(wc.size(), 1U);
        EXPECT_EQ(data.Portals()[wc[0]].id, Intern("P_HALL__WC1"));
    }

    TEST_F(WorldDataTest, AnUnknownCellHasAnEmptyGroupingRatherThanAnError)
    {
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);
        EXPECT_TRUE(world.Value().PortalsOf(Intern("L9_NOWHERE")).empty());
        EXPECT_TRUE(world.Value().LightsOf(Intern("L9_NOWHERE")).empty());
        EXPECT_TRUE(world.Value().PropsOf(Intern("L9_NOWHERE")).empty());
        // And so does a cell that exists and simply has none, which is the same answer to a caller
        // that is going to iterate it either way.
        EXPECT_TRUE(world.Value().PropsOf(Intern("L0_FOYER")).empty());
    }

    TEST_F(WorldDataTest, RowsKeepTheirAuthoredOrderInsideACell)
    {
        // `LIGHT_HALL` is row 0 and `LIGHT_HALL_2` is row 2, with a foyer light between them. The
        // grouping must not reorder them: the order things are solved and drawn in is the order
        // they were authored, so a frame capture diff reflects a data change and not a hash seed.
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);
        const world::WorldData& data = world.Value();

        const auto hall = data.LightsOf(Intern("L0_HALL"));
        ASSERT_EQ(hall.size(), 2U);
        EXPECT_EQ(data.Lights()[hall[0]].id, Intern("LIGHT_HALL"));
        EXPECT_EQ(data.Lights()[hall[1]].id, Intern("LIGHT_HALL_2"));
    }

    TEST_F(WorldDataTest, OtherSideCrossesAPortalAndRefusesACellItDoesNotTouch)
    {
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);
        const world::WorldData& data = world.Value();
        const world::Portal* portal = data.FindPortal(Intern("P_HALL__WC1"));
        ASSERT_NE(portal, nullptr);

        EXPECT_EQ(data.OtherSide(*portal, Intern("L0_HALL")), Intern("L0_WC1"));
        EXPECT_EQ(data.OtherSide(*portal, Intern("L0_WC1")), Intern("L0_HALL"));
        EXPECT_FALSE(data.OtherSide(*portal, Intern("L0_FOYER")).IsValid());
    }

    // --- geometry -------------------------------------------------------------------------------

    TEST_F(WorldDataTest, AnExtentComesFromTheLevelUnlessTheCellOverridesIt)
    {
        auto contents = Fixture();
        contents.cells[0].yOverride = world::Extent{0.60F, 3.65F};

        auto world = world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(world);
        const world::WorldData& data = world.Value();

        const auto overridden = data.ExtentOf(*data.FindCell(Intern("L0_FOYER")));
        ASSERT_TRUE(overridden);
        EXPECT_FLOAT_EQ(overridden.Value().ceilingY, 3.65F);

        const auto inherited = data.ExtentOf(*data.FindCell(Intern("L0_HALL")));
        ASSERT_TRUE(inherited);
        EXPECT_FLOAT_EQ(inherited.Value().floorY, 0.60F);
        EXPECT_FLOAT_EQ(inherited.Value().ceilingY, 3.30F);
        EXPECT_FLOAT_EQ(inherited.Value().Height(), 2.70F);
    }

    TEST_F(WorldDataTest, ARafterBoundedLevelHasNoExtentToInvent)
    {
        // `L3`'s ceiling is null because the attic is bounded by rafters. Returning `ffl..ffl` or
        // `ffl..ridgeY` would put a ceiling slab through the roof and look like a working answer,
        // so the model refuses instead and the message says what to author.
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);
        const world::WorldData& data = world.Value();

        const auto extent = data.ExtentOf(*data.FindCell(Intern("L3_STORE")));
        ASSERT_FALSE(extent);
        EXPECT_EQ(extent.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(extent.Error().Message().find("yOverride"), std::string::npos) << extent.Error().ToString();
    }

    TEST_F(WorldDataTest, ACellOnAMissingLevelIsNotFoundRatherThanZero)
    {
        auto contents = Fixture();
        contents.cells[2].level = Intern("L7");

        auto world = world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(world);
        const auto extent = world.Value().ExtentOf(*world.Value().FindCell(Intern("L0_WC1")));
        ASSERT_FALSE(extent);
        EXPECT_EQ(extent.Error().Code(), ErrorCode::NotFound);
    }

    TEST_F(WorldDataTest, ContainsFollowsAnLShapeAndNotItsBoundingBox)
    {
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);
        const world::WorldData& data = world.Value();
        const world::Cell& hall = *data.FindCell(Intern("L0_HALL"));

        EXPECT_TRUE(data.CellContains(hall, Vector3(0.0F, 1.5F, 6.0F))) << "in the long arm";
        EXPECT_TRUE(data.CellContains(hall, Vector3(4.0F, 1.5F, 9.0F))) << "in the short arm";
        // Inside the bounding box of the two, and inside neither of them. A single box would
        // wrongly say yes here, which is the whole reason a cell carries a list.
        EXPECT_FALSE(data.CellContains(hall, Vector3(4.0F, 1.5F, 5.0F)));
    }

    TEST_F(WorldDataTest, ContainsChecksTheVerticalExtentToo)
    {
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);
        const world::WorldData& data = world.Value();
        const world::Cell& hall = *data.FindCell(Intern("L0_HALL"));

        EXPECT_FALSE(data.CellContains(hall, Vector3(0.0F, 0.0F, 6.0F))) << "below the floor";
        EXPECT_FALSE(data.CellContains(hall, Vector3(0.0F, 4.0F, 6.0F))) << "above the ceiling";
        EXPECT_TRUE(data.CellContains(hall, Vector3(0.0F, 0.60F, 6.0F))) << "on the floor";
    }

    TEST_F(WorldDataTest, TheHysteresisMarginIsHonouredInBothAxes)
    {
        // §16.4's 5 cm. Without it a player standing exactly on a boundary flips between two cells
        // every frame and every system keyed off the current cell churns with them.
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);
        const world::WorldData& data = world.Value();
        const world::Cell& hall = *data.FindCell(Intern("L0_HALL"));

        const Vector3 justOutside(2.03F, 1.5F, 6.0F);
        EXPECT_FALSE(data.CellContains(hall, justOutside));
        EXPECT_TRUE(data.CellContains(hall, justOutside, 0.05F));

        const Vector3 justBelow(0.0F, 0.57F, 6.0F);
        EXPECT_FALSE(data.CellContains(hall, justBelow));
        EXPECT_TRUE(data.CellContains(hall, justBelow, 0.05F));
    }

    TEST_F(WorldDataTest, FootprintAreaSumsEveryBox)
    {
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);
        const world::Cell& hall = *world.Value().FindCell(Intern("L0_HALL"));
        // 4 x 6 plus 4 x 2.
        EXPECT_FLOAT_EQ(world::WorldData::FootprintArea(hall), 32.0F);
    }

    // --- the vocabularies -----------------------------------------------------------------------

    TEST_F(WorldDataTest, EveryVocabularyRoundTripsThroughItsSpelling)
    {
        // Both directions exist because a diagnostic that says "kind 3" is one nobody can act on.
        // Walking every value is what makes "one table, read twice" true rather than intended: a
        // value added to the enum and forgotten in the table fails here and nowhere else.
        auto check = [](auto value, auto parse)
        {
            const std::string_view text = world::ToStringView(value);
            EXPECT_NE(text, "?") << "a value has no spelling";
            const auto parsed = parse(text);
            ASSERT_TRUE(parsed) << parsed.Error().ToString();
            EXPECT_EQ(parsed.Value(), value) << text;
        };

        for (const auto value : {world::CellKind::Room,
                                 world::CellKind::Corridor,
                                 world::CellKind::Stair,
                                 world::CellKind::Closet,
                                 world::CellKind::Garage,
                                 world::CellKind::Exterior,
                                 world::CellKind::Void})
        {
            check(value, world::ParseCellKind);
        }
        for (const auto value : {world::PortalKind::CasedOpening,
                                 world::PortalKind::Door,
                                 world::PortalKind::DoubleDoor,
                                 world::PortalKind::Slider,
                                 world::PortalKind::Window,
                                 world::PortalKind::GarageDoor,
                                 world::PortalKind::StairWell,
                                 world::PortalKind::ExteriorDoor,
                                 world::PortalKind::Hatch})
        {
            check(value, world::ParsePortalKind);
        }
        for (const auto value : {world::PortalOpacity::Open,
                                 world::PortalOpacity::OpaqueWhenClosed,
                                 world::PortalOpacity::Translucent,
                                 world::PortalOpacity::Glass})
        {
            check(value, world::ParsePortalOpacity);
        }
        for (const auto value : {world::PlaneAxis::X, world::PlaneAxis::Y, world::PlaneAxis::Z})
        {
            check(value, world::ParsePlaneAxis);
        }
        for (const auto value : {world::LightType::Point,
                                 world::LightType::Spot,
                                 world::LightType::Directional,
                                 world::LightType::AreaProxy,
                                 world::LightType::EmissiveOnly})
        {
            check(value, world::ParseLightType);
        }
        for (const auto value : {world::EffectTier::Basic,
                                 world::EffectTier::DualTexture,
                                 world::EffectTier::AlphaTest,
                                 world::EffectTier::Skinned})
        {
            check(value, world::ParseEffectTier);
        }
        for (const auto value : {world::AlphaMode::Opaque, world::AlphaMode::Mask, world::AlphaMode::Blend})
        {
            check(value, world::ParseAlphaMode);
        }
        for (const auto value :
             {world::PropCollision::Proxy, world::PropCollision::None, world::PropCollision::Box})
        {
            check(value, world::ParsePropCollision);
        }
        for (const auto value : {world::VisibilityHint::Opaque, world::VisibilityHint::Open})
        {
            check(value, world::ParseVisibilityHint);
        }
        for (const auto value : {world::Orientation::N,
                                 world::Orientation::NE,
                                 world::Orientation::E,
                                 world::Orientation::SE,
                                 world::Orientation::S,
                                 world::Orientation::SW,
                                 world::Orientation::W,
                                 world::Orientation::NW})
        {
            check(value, world::ParseOrientation);
        }
        for (const auto value : {world::MarkerKind::Perch, world::MarkerKind::Bed, world::MarkerKind::Bowl})
        {
            EXPECT_NE(world::ToStringView(value), "?");
        }
    }

    TEST_F(WorldDataTest, AnUnknownVocabularyValueNamesTheAlternatives)
    {
        const auto parsed = world::ParseCellKind("conservatory");
        ASSERT_FALSE(parsed);
        EXPECT_EQ(parsed.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(parsed.Error().Message().find("conservatory"), std::string::npos);
        EXPECT_NE(parsed.Error().Message().find("corridor"), std::string::npos)
            << "the message must list the vocabulary: " << parsed.Error().ToString();
    }

    TEST_F(WorldDataTest, TheSpellingsAreTheOnesTheFilesUse)
    {
        // Not a tautology against the table: these are the literal strings in
        // `docs/world-format.md` and in `docs/world-schema/*.schema.json`, so a rename on either
        // side is a load failure that this catches at build time instead.
        EXPECT_EQ(world::ToStringView(world::PortalKind::CasedOpening), "cased_opening");
        EXPECT_EQ(world::ToStringView(world::PortalKind::StairWell), "stair_well");
        EXPECT_EQ(world::ToStringView(world::PortalOpacity::OpaqueWhenClosed), "opaque_when_closed");
        EXPECT_EQ(world::ToStringView(world::LightType::AreaProxy), "area_proxy");
        EXPECT_EQ(world::ToStringView(world::EffectTier::DualTexture), "DualTexture");
        EXPECT_EQ(world::ToStringView(world::CellKind::Exterior), "exterior");
    }

    TEST_F(WorldDataTest, PassabilityAgreesWithTheValidatorAndTheGraphReport)
    {
        // `validate_world.py` rule 5, `report_graph.py` and this must give the same answer or the
        // validator is passing a house the game cannot be walked around. A window is never a way
        // through; a cased opening and a stair well are open with nobody touching anything.
        EXPECT_FALSE(world::IsPassable(world::PortalKind::Window));
        EXPECT_TRUE(world::IsPassable(world::PortalKind::Door));
        EXPECT_TRUE(world::IsPassable(world::PortalKind::StairWell));
        EXPECT_TRUE(world::IsPassable(world::PortalKind::Hatch));

        EXPECT_TRUE(world::IsAlwaysOpen(world::PortalKind::CasedOpening));
        EXPECT_TRUE(world::IsAlwaysOpen(world::PortalKind::StairWell));
        EXPECT_FALSE(world::IsAlwaysOpen(world::PortalKind::Door));
        EXPECT_FALSE(world::IsAlwaysOpen(world::PortalKind::GarageDoor));
    }

    TEST_F(WorldDataTest, SpeciesIsASetAndNotAPairOfBooleans)
    {
        EXPECT_TRUE(Includes(world::Species::Both, world::Species::Dog));
        EXPECT_TRUE(Includes(world::Species::Both, world::Species::Cat));
        EXPECT_FALSE(Includes(world::Species::Dog, world::Species::Cat));
        EXPECT_FALSE(Includes(world::Species::None, world::Species::Dog));
        EXPECT_EQ(world::Species::Dog | world::Species::Cat, world::Species::Both);

        const auto dog = world::ParseSpecies("dog");
        ASSERT_TRUE(dog);
        EXPECT_EQ(dog.Value(), world::Species::Dog);
        EXPECT_FALSE(world::ParseSpecies("ferret"));
    }

    // --- the derived numbers a later rule leans on ----------------------------------------------

    TEST_F(WorldDataTest, AFlightKnowsHowFarItClimbsAndWhetherItIsClimbable)
    {
        world::StairFlight flight;
        flight.risers = 17;
        flight.rise = 3.05F / 17.0F;
        flight.going = 0.280F;

        EXPECT_NEAR(flight.Climb(), 3.05F, 1e-4F);
        EXPECT_NEAR(flight.Blondel(), 0.6388F, 1e-3F);
        // §70.5's comfortable range, which `validate_world.py` rule 10 checks against the same two
        // numbers. Both sides computing it the same way is the point of putting it here.
        EXPECT_GE(flight.Blondel(), 0.600F);
        EXPECT_LE(flight.Blondel(), 0.650F);
    }

    TEST_F(WorldDataTest, APortalKnowsItsOwnOpening)
    {
        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);
        const world::Portal& door = *world.Value().FindPortal(Intern("P_HALL__WC1"));
        EXPECT_NEAR(door.Width(), 0.90F, 1e-5F);
        EXPECT_NEAR(door.Height(), 2.02F, 1e-5F);
    }

    TEST_F(WorldDataTest, TheModelIsMovableAndNotCopyable)
    {
        // Immutability is worth nothing if a caller can take a copy and let the two drift, and a
        // copy of the whole house is a copy nobody meant to pay for either.
        static_assert(!std::is_copy_constructible_v<world::WorldData>);
        static_assert(std::is_move_constructible_v<world::WorldData>);

        auto world = world::WorldData::Create(Fixture());
        ASSERT_TRUE(world);
        const std::size_t before = world.Value().Size();
        world::WorldData moved = std::move(world.Value());
        EXPECT_EQ(moved.Size(), before);
        EXPECT_NE(moved.FindCell(Intern("L0_HALL")), nullptr);
    }
} // namespace
