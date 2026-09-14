// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Result.hpp"
#include "cnahouse/weather/WeatherState.hpp"
#include "cnahouse/world/InteractableExpr.hpp"

/// @file
/// The row types of the world files, as the runtime holds them (`HOUSE-00342`, `cna-house.md` §15).
///
/// One struct per row of one authored file, in the same order the file writes them, so that a
/// reader with `docs/world-format.md` open can follow both at once. Three things change on the way
/// in and each is deliberate:
///
/// * every **id** becomes a `util::Id` — a 32-bit interned hash — because ids are compared on every
///   physics step and every camera move, and a string compare there is a string compare a hundred
///   thousand times a second. The name survives in `util::IdRegistry::NameOf` so a log line can
///   still say `L0_KITCHEN`;
/// * every **enum-valued string** becomes an enum, parsed once at load, so that an unknown value is
///   a load-time error naming the file and the field rather than a silent default at frame 4000;
/// * `null` becomes `std::optional`, never a sentinel number. `docs/world-format.md` is explicit
///   that `null` means "use the documented default" and is **never** a synonym for zero, and an
///   `optional` is the only spelling that cannot be confused with a real value.
///
/// Nothing here allocates after load and nothing here is mutable: see `WorldData`.

namespace cnahouse::world
{

    /// @brief A footprint rectangle, `{"x": [min, max], "z": [min, max]}` in metres.
    ///
    /// Not a `BoundingBox`: a cell's footprint is authored in two axes and its vertical extent
    /// comes from the level or from `yOverride`, so a 3-D box here would have to invent a Y before
    /// the level table has been read.
    struct Footprint
    {
        float minX = 0.0F;
        float maxX = 0.0F;
        float minZ = 0.0F;
        float maxZ = 0.0F;

        [[nodiscard]] constexpr bool Contains(float x, float z, float margin = 0.0F) const noexcept
        {
            return x >= minX - margin && x <= maxX + margin && z >= minZ - margin && z <= maxZ + margin;
        }

        [[nodiscard]] constexpr float Area() const noexcept
        {
            return (maxX - minX) * (maxZ - minZ);
        }
    };

    /// @brief A cell's floor and ceiling Y, resolved from `yOverride` or from its level.
    struct Extent
    {
        float floorY = 0.0F;
        float ceilingY = 0.0F;

        [[nodiscard]] constexpr bool Contains(float y, float margin = 0.0F) const noexcept
        {
            return y >= floorY - margin && y <= ceilingY + margin;
        }

        [[nodiscard]] constexpr float Height() const noexcept
        {
            return ceilingY - floorY;
        }
    };

    // ------------------------------------------------------------------------------- vocabularies

    enum class CellKind : std::uint8_t
    {
        Room,
        Corridor,
        Stair,
        Closet,
        Garage,
        Exterior,
        Void,
    };

    enum class PortalKind : std::uint8_t
    {
        CasedOpening,
        Door,
        DoubleDoor,
        Slider,
        Window,
        GarageDoor,
        StairWell,
        ExteriorDoor,
        Hatch,
    };

    enum class PortalOpacity : std::uint8_t
    {
        Open,
        OpaqueWhenClosed,
        Translucent,
        Glass,
    };

    /// @brief Which axis a portal's plane is perpendicular to.
    ///
    /// `Y` is a horizontal plane: a stair well or a hatch. Both are in the portal vocabulary and
    /// neither is a hole in a wall, and without this case no stair could join the floors it climbs
    /// (`HOUSE-00358`). On `Y`, `u` is world X and `v` is world Z.
    enum class PlaneAxis : std::uint8_t
    {
        X,
        Y,
        Z,
    };

    enum class OpeningKind : std::uint8_t
    {
        Door,
        Window,
    };

    enum class HingeSide : std::uint8_t
    {
        Left,
        Right,
    };

    enum class LightType : std::uint8_t
    {
        Point,
        Spot,
        Directional,
        AreaProxy,
        EmissiveOnly,
    };

    /// @brief §22.2's closed class vocabulary.
    ///
    /// Closed and parsed once, because the class is what the effect tier, the footstep sound and
    /// the snow response all fall back to. A free-text class would make every one of those
    /// fallbacks silently pick a default for a typo.
    enum class MaterialClass : std::uint8_t
    {
        Paint,
        Wood,
        Carpet,
        Tile,
        Stone,
        Concrete,
        Metal,
        Plastic,
        Glass,
        Fabric,
        Skin,
        Hair,
        Fur,
        Foliage,
        Asphalt,
        Gravel,
        Grass,
        Soil,
        Water,
        Emissive,
    };

    /// @brief §22.2's `wet_<class>` and `snow_<class>` forms, as a modifier rather than 60 classes.
    ///
    /// They are derived spellings of the same class -- `wet_wood` is wood with a darkened albedo,
    /// not a different material class -- so the base and the state are read apart. That is also
    /// what lets the effect-tier fallback consult one table of twenty rows and not three.
    enum class SurfaceState : std::uint8_t
    {
        Dry,
        Wet,
        Snowy,
    };

    enum class AlphaMode : std::uint8_t
    {
        Opaque,
        Mask,
        Blend,
    };

    /// @brief The Tier S effect a material asks for: XNA 4.0's four stock effects and no others.
    enum class EffectTier : std::uint8_t
    {
        Basic,
        DualTexture,
        AlphaTest,
        Skinned,
    };

    enum class PropCollision : std::uint8_t
    {
        Proxy,
        None,
        Box,
    };

    enum class VisibilityHint : std::uint8_t
    {
        Opaque,
        Open,
    };

    enum class Orientation : std::uint8_t
    {
        N,
        NE,
        E,
        SE,
        S,
        SW,
        W,
        NW,
    };

    /// @brief Which animals an edge, perch, bed or bowl is for.
    ///
    /// A bitmask rather than an enum because §61's answer for most of the graph is "both", and a
    /// pair of booleans would have to be kept in step by hand at every call site.
    enum class Species : std::uint8_t
    {
        None = 0,
        Dog = 1 << 0,
        Cat = 1 << 1,
        Both = Dog | Cat,
    };

    [[nodiscard]] constexpr Species operator|(Species a, Species b) noexcept
    {
        return static_cast<Species>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
    }

    [[nodiscard]] constexpr bool Includes(Species set, Species one) noexcept
    {
        return (static_cast<std::uint8_t>(set) & static_cast<std::uint8_t>(one)) != 0;
    }

    /// @brief Which of the three pet markers a `NavMarker` is.
    enum class MarkerKind : std::uint8_t
    {
        Perch,
        Bed,
        Bowl,
    };

    // The stable spelling of every vocabulary, and the parse back. Both directions exist because a
    // diagnostic that says "kind 3" is a diagnostic nobody can act on, and because a round trip is
    // the only cheap way to prove the two tables agree.
    [[nodiscard]] std::string_view ToStringView(CellKind value) noexcept;
    [[nodiscard]] std::string_view ToStringView(PortalKind value) noexcept;
    [[nodiscard]] std::string_view ToStringView(PortalOpacity value) noexcept;
    [[nodiscard]] std::string_view ToStringView(PlaneAxis value) noexcept;
    [[nodiscard]] std::string_view ToStringView(OpeningKind value) noexcept;
    [[nodiscard]] std::string_view ToStringView(HingeSide value) noexcept;
    [[nodiscard]] std::string_view ToStringView(LightType value) noexcept;
    [[nodiscard]] std::string_view ToStringView(AlphaMode value) noexcept;
    [[nodiscard]] std::string_view ToStringView(EffectTier value) noexcept;
    [[nodiscard]] std::string_view ToStringView(PropCollision value) noexcept;
    [[nodiscard]] std::string_view ToStringView(VisibilityHint value) noexcept;
    [[nodiscard]] std::string_view ToStringView(Orientation value) noexcept;
    [[nodiscard]] std::string_view ToStringView(MarkerKind value) noexcept;

    [[nodiscard]] util::Result<CellKind> ParseCellKind(std::string_view text);
    [[nodiscard]] util::Result<PortalKind> ParsePortalKind(std::string_view text);
    [[nodiscard]] util::Result<PortalOpacity> ParsePortalOpacity(std::string_view text);
    [[nodiscard]] util::Result<PlaneAxis> ParsePlaneAxis(std::string_view text);
    [[nodiscard]] util::Result<OpeningKind> ParseOpeningKind(std::string_view text);
    [[nodiscard]] util::Result<HingeSide> ParseHingeSide(std::string_view text);
    [[nodiscard]] util::Result<LightType> ParseLightType(std::string_view text);
    [[nodiscard]] util::Result<AlphaMode> ParseAlphaMode(std::string_view text);
    [[nodiscard]] util::Result<EffectTier> ParseEffectTier(std::string_view text);
    [[nodiscard]] util::Result<PropCollision> ParsePropCollision(std::string_view text);
    [[nodiscard]] util::Result<VisibilityHint> ParseVisibilityHint(std::string_view text);
    [[nodiscard]] util::Result<Orientation> ParseOrientation(std::string_view text);
    [[nodiscard]] util::Result<Species> ParseSpecies(std::string_view text);

    [[nodiscard]] std::string_view ToStringView(MaterialClass value) noexcept;
    [[nodiscard]] std::string_view ToStringView(SurfaceState value) noexcept;

    /// @brief A `class` string as its base class and its surface state.
    struct MaterialClassSpec
    {
        MaterialClass base = MaterialClass::Paint;
        SurfaceState state = SurfaceState::Dry;
    };

    /// @brief Parses `tile`, `wet_tile` or `snow_tile`.
    [[nodiscard]] util::Result<MaterialClassSpec> ParseMaterialClass(std::string_view text);

    /// @brief The spelling `ParseMaterialClass` accepts, for a diagnostic or a round trip.
    [[nodiscard]] std::string SpellMaterialClass(MaterialClassSpec spec);

    /// @brief §22.2's Tier S effect for a class, the documented fallback when `effectTierS` is
    /// absent.
    ///
    /// The **static** reading of the table: `wood` is `DualTextureEffect` static and `BasicEffect`
    /// dynamic, and only static props are batched (§17.4), so the static column is the one the
    /// batcher uses. `tools/world/build_chunks.py` reads the same table for the same reason, and a
    /// unit test asserts the two agree row for row -- a chunk built with one vertex layout and
    /// drawn with the effect the other chose is a wrong-looking surface nobody can trace.
    [[nodiscard]] EffectTier DefaultEffectTier(MaterialClass value) noexcept;

    /// @brief Whether a person can walk through a portal of this kind when nothing is holding it.
    ///
    /// A window is never a way through, whatever its opacity: it is a hole for light and for sound.
    /// `validate_world.py` rule 5 and `report_graph.py` make the same distinction, and the three
    /// must agree or the validator is passing a house the game cannot be walked around.
    [[nodiscard]] constexpr bool IsPassable(PortalKind kind) noexcept
    {
        return kind != PortalKind::Window;
    }

    /// @brief Whether a portal of this kind is open whether or not anybody has touched anything.
    [[nodiscard]] constexpr bool IsAlwaysOpen(PortalKind kind) noexcept
    {
        return kind == PortalKind::CasedOpening || kind == PortalKind::StairWell;
    }

    // -------------------------------------------------------------------------------- the rows --

    /// @brief `layout.levels.json`: one storey.
    struct Level
    {
        util::Id id;
        std::string name;
        float ffl = 0.0F;
        /// The underside of the ceiling above. Absent on a rafter-bounded level: `L3` is bounded by
        /// `roof`, not by a plane, and a cell there must carry its own `yOverride`.
        std::optional<float> ceiling;
        float structureDepth = 0.0F;
        util::Id roof;
    };

    /// @brief `layout.levels.json` `construction`: §12.2's constants, in metres.
    struct Construction
    {
        float wallExterior = 0.0F;
        float wallPartition = 0.0F;
        float wallPlumbing = 0.0F;
        float wallGarage = 0.0F;
        float foundationWall = 0.0F;
        float kneeWallHeight = 0.0F;
        float ridgeY = 0.0F;
        float roofPitch = 0.0F;
        float skirting = 0.0F;
        float cornice = 0.0F;
        float balustrade = 0.0F;
        float railing = 0.0F;
    };

    /// @brief `layout.levels.json` `plumbing.stacks`: one of §12.5's STACK-A…F.
    struct PlumbingStack
    {
        util::Id id;
        std::vector<util::Id> cells;
        Footprint chase;
        util::Id dropTo;
    };

    //! One supply register: the grille §62.6 places the duct rumble and tick at.
    struct HvacRegister
    {
        util::Id cell;
        Microsoft::Xna::Framework::Vector3 position;
        //! `floor` upstairs, where the branch runs in the joist space below; `ceiling` in the
        //! basement, where the trunk is exposed under the L0 slab -- which is why §62.6's rumble
        //! is loudest down there.
        std::string kind;
    };

    struct HvacBranch
    {
        util::Id id;
        std::vector<util::Id> cells;
        std::string trunk;
        std::vector<HvacRegister> registers;
    };

    struct CellAcoustic
    {
        util::Id roomTone;
        float absorption = 0.0F;
        std::string reverbHint;
    };

    struct CellThermal
    {
        bool heated = false;
        util::Id ductBranch;
    };

    struct CellDaylight
    {
        std::vector<util::Id> windowIds;
        std::optional<Orientation> orientation;
        float exposure = 0.0F;
    };

    /// @brief One baked irradiance texture generated for a cell's shell receivers.
    struct CellLightmapTexture
    {
        std::string contentName;
        /// @brief Multiplier that restores the HDR peak removed before the PNG was written.
        float scale = 1.0F;
    };

    struct CellLightmapGroup
    {
        util::Id group;
        CellLightmapTexture texture;
    };

    /// @brief Generated §18.3 lightmap bindings, stored beside the cell they illuminate.
    struct CellLightmaps
    {
        /// @brief SHA-256 of the receiver shell used by the bake, including the `sha256:` prefix.
        std::string shellHash;
        std::optional<CellLightmapTexture> daylight;
        std::vector<CellLightmapGroup> artificial;
    };

    /// @brief `layout.cells.json`: the unit of visibility, audio, lighting and residency.
    struct Cell
    {
        util::Id id;
        util::Id level;
        std::string name;
        CellKind kind = CellKind::Room;
        std::vector<Footprint> boxes;
        std::optional<Extent> yOverride;
        util::Id floorMaterial;
        util::Id wallMaterial;
        util::Id ceilingMaterial;
        /// @brief Non-lightmapped joinery finish for skirtings, casings and room-side leaves.
        util::Id trimMaterial;
        std::string footstepSurface;
        CellAcoustic acoustic;
        CellThermal thermal;
        std::vector<util::Id> lightGroups;
        CellDaylight daylight;
        CellLightmaps lightmaps;
        std::string residencyPack;
        std::int32_t lodBias = 0;
        VisibilityHint visibilityHint = VisibilityHint::Opaque;
        util::Id navMeshRegion;
        /// @brief The cell this one is nested in, for a container sub-cell.
        ///
        /// §54: "a container is a tiny sub-cell with its own portal, so this falls out of the
        /// visibility system rather than being a special case". The fridge and freezer interiors
        /// (§56.1) are cells inside the kitchen and the pantry; declaring the parent is what lets
        /// rule 3 tell that from two rooms drawn on top of each other, and what would otherwise
        /// force each of §54's 214 containers to be carved out of the room around it.
        util::Id parent;
    };

    /// @brief `layout.portals.json`: an axis-aligned rectangle on an axis-aligned plane.
    struct Portal
    {
        util::Id id;
        util::Id cellA;
        util::Id cellB;
        PlaneAxis axis = PlaneAxis::X;
        float planeValue = 0.0F;
        /// The rectangle in the plane. On `X` and `Z`, `u` is the other horizontal axis and `v` is
        /// world Y; on `Y`, `u` is world X and `v` is world Z.
        float minU = 0.0F;
        float maxU = 0.0F;
        float minV = 0.0F;
        float maxV = 0.0F;
        PortalKind kind = PortalKind::CasedOpening;
        util::Id aperture;
        PortalOpacity opacity = PortalOpacity::Open;
        std::optional<std::int32_t> maxDepth;
        float soundLossOpen = 0.0F;
        float soundLossClosed = 0.0F;
        /// §70.5 exempts a deliberately low portal from the 1.95 m capsule clearance.
        bool crouch = false;

        [[nodiscard]] constexpr float Width() const noexcept
        {
            return maxU - minU;
        }

        [[nodiscard]] constexpr float Height() const noexcept
        {
            return maxV - minV;
        }
    };

    struct Leaf
    {
        float width = 0.0F;
        float height = 0.0F;
        float thickness = 0.0F;
    };

    /// @brief `layout.openings.json`: a door or a window as geometry plus entity.
    struct Opening
    {
        util::Id id;
        OpeningKind kind = OpeningKind::Door;
        //! §12.3's and §12.6's schedule entry -- `D_INT_PASSAGE`, `W_DH_STD`. Id-shaped and not an
        //! id: many openings share one. Rules ask what an opening *is* where a measurement would be
        //! the wrong question, §70.5's leaf band being the case that forced it.
        util::Id type;
        util::Id portal;
        Leaf leaf;
        std::optional<HingeSide> hinge;
        //! The cell the leaf opens into, by id. A reference, not prose: the format wrote it
        //! `"into_L0_WC1"` until `HOUSE-00378`, which nothing could resolve.
        util::Id swing;
        float maxAngleDeg = 0.0F;
        util::Id frameAsset;
        float casing = 0.0F;
        util::Id asset;
        util::Id material;
        bool solid = false;
        bool lockable = false;
    };

    struct Landing
    {
        std::int32_t at = 0;
        float depth = 0.0F;
    };

    /// @brief `layout.stairs.json`: one flight.
    struct StairFlight
    {
        util::Id id;
        util::Id fromCell;
        util::Id toCell;
        std::int32_t risers = 0;
        float rise = 0.0F;
        float going = 0.0F;
        float width = 0.0F;
        std::vector<Landing> landings;
        bool collisionRamp = false;
        std::string surface;
        //! The heights a flight starts and ends at, for a flight between two cells on ONE level --
        //! the porch, terrace and garage steps. Empty on a flight between storeys, where the
        //! levels' FFLs say what it climbs (`HOUSE-00379`).
        std::optional<float> fromY;
        std::optional<float> toY;

        /// @brief `risers × rise`: how far the flight actually climbs.
        [[nodiscard]] constexpr float Climb() const noexcept
        {
            return static_cast<float>(risers) * rise;
        }

        /// @brief §70.5's `2·rise + going`, the number that says whether a stair is climbable.
        [[nodiscard]] constexpr float Blondel() const noexcept
        {
            return 2.0F * rise + going;
        }
    };

    /// @brief One piece of a flight's collision ramp: a sloped run, or a flat landing.
    ///
    /// `build_collision.py` builds exactly this segmentation offline -- a closed wedge per run and
    /// a box per landing -- and the runtime needs the same numbers to place a step sound, to know
    /// how far along a flight the player is, and to answer §70.5's headroom question. Deriving them
    /// in one place, from the authored row, is what stops the two drifting.
    struct StairSegment
    {
        bool isLanding = false;
        /// The riser this segment starts at, counting from the bottom.
        std::int32_t fromRiser = 0;
        /// Risers climbed by this segment; 0 for a landing.
        std::int32_t risers = 0;
        /// Horizontal length along the run, in metres.
        float length = 0.0F;
        /// Vertical gain, in metres; 0 for a landing.
        float height = 0.0F;
    };

    /// @brief Splits a flight into its runs and landings, bottom first.
    ///
    /// The same walk `build_collision.py` does: consume risers until the next landing, emit the
    /// run, emit the landing, repeat. A landing at riser 0 or at the top riser is not a landing in
    /// the middle of a flight and produces no zero-length run.
    [[nodiscard]] std::vector<StairSegment> SegmentFlight(const StairFlight& flight);

    /// @brief The flight's total horizontal run: `risers × going` plus every landing's depth.
    [[nodiscard]] float TotalRun(const StairFlight& flight);

    /// @brief `layout.lights.json`: one light.
    struct Light
    {
        util::Id id;
        util::Id cell;
        util::Id group;
        LightType type = LightType::Point;
        Microsoft::Xna::Framework::Vector3 position;
        Microsoft::Xna::Framework::Vector3 direction;
        float colorK = 2700.0F;
        float intensityLm = 0.0F;
        float range = 0.0F;
        float coneInnerDeg = 0.0F;
        float coneOuterDeg = 0.0F;
        util::Id fixtureProp;
        std::string emissiveMaterialSlot;
        bool castsBlobShadow = false;
        bool bakedIntoLightmap = false;
        bool defaultOn = false;
    };

    struct WetResponse
    {
        float albedoDarken = 0.0F;
        float specularBoost = 0.0F;
        float powerBoost = 0.0F;
    };

    struct SnowResponse
    {
        bool coverable = false;
        float slopeLimitDeg = 0.0F;
    };

    /// @brief `layout.materials.json`: §22's material definition.
    struct MaterialDef
    {
        util::Id id;
        MaterialClass materialClass = MaterialClass::Paint;
        SurfaceState surfaceState = SurfaceState::Dry;
        std::string albedo;
        std::string normal;
        std::int32_t lightmapChannel = 0;
        Microsoft::Xna::Framework::Vector3 tint{1.0F, 1.0F, 1.0F};
        Microsoft::Xna::Framework::Vector3 specularColor;
        float specularPower = 0.0F;
        AlphaMode alphaMode = AlphaMode::Opaque;
        float alpha = 1.0F;
        std::optional<float> alphaCutoff;
        bool twoSided = false;
        float uvScaleU = 1.0F;
        float uvScaleV = 1.0F;
        WetResponse wet;
        SnowResponse snow;
        std::string footstepSurface;
        float audioAbsorption = 0.0F;
        EffectTier effectTierS = EffectTier::Basic;
        std::string effectTierE;
    };

    /// @brief `layout.props.json`: one placement.
    struct Prop
    {
        util::Id id;
        util::Id asset;
        util::Id cell;
        Microsoft::Xna::Framework::Vector3 position;
        float yawDeg = 0.0F;
        float scale = 1.0F;
        bool isStatic = true;
        util::Id lodGroup;
        PropCollision collision = PropCollision::Proxy;
        util::Id material;
        util::Id interactable;
        /// The §12.5 stack this fixture drains to, if it is a fixture at all.
        util::Id plumbing;
    };

    struct NavNode
    {
        util::Id id;
        util::Id cell;
        Microsoft::Xna::Framework::Vector3 position;
        std::string kind;
    };

    struct NavEdge
    {
        util::Id a;
        util::Id b;
        util::Id portal;
        float cost = 0.0F;
        Species species = Species::Both;
    };

    /// @brief A perch, a bed or a bowl: three files' worth of the same three fields.
    struct NavMarker
    {
        util::Id id;
        MarkerKind kind = MarkerKind::Perch;
        util::Id cell;
        util::Id prop;
        Microsoft::Xna::Framework::Vector3 position;
        Species species = Species::Both;
    };

    struct NavForbidden
    {
        util::Id cell;
        Species species = Species::None;
    };

    struct AudioZone
    {
        util::Id id;
        util::Id cell;
        util::Id bed;
        float gain = 1.0F;
    };

    struct AudioEmitter
    {
        util::Id id;
        util::Id cell;
        Microsoft::Xna::Framework::Vector3 position;
        util::Id loop;
        float gain = 1.0F;
        float radius = 0.0F;
        util::Id interactable;
    };

    /// @brief One row of `layout.audio.json`'s `transmission` table.
    ///
    /// A **named** loss pair -- `door_hollow`, `door_solid` -- rather than a per-portal number,
    /// because §64.3's figures are properties of a kind of construction and the house has 62
    /// doors of half a dozen kinds. A portal's own `soundLoss` overrides it where a door is
    /// unusual; this is what the other sixty read.
    ///
    /// A vector rather than a map: it is read once per portal solve, it has a handful of rows, and
    /// a stable order keeps a diagnostic that lists it stable too.
    struct AudioTransmission
    {
        std::string kind;
        float open = 0.0F;
        float closed = 0.0F;
    };

    struct Terrain
    {
        std::string heightfield;
        float sizeX = 0.0F;
        float sizeZ = 0.0F;
        Microsoft::Xna::Framework::Vector3 origin;
        float yScale = 1.0F;
        util::Id material;
    };

    struct Road
    {
        std::vector<Microsoft::Xna::Framework::Vector3> centreline;
        float width = 0.0F;
        util::Id material;
    };

    struct Fence
    {
        util::Id id;
        util::Id asset;
        std::vector<Microsoft::Xna::Framework::Vector3> path;
        float height = 0.0F;
        util::Id gate;
    };

    struct NeighbourBuilding
    {
        util::Id id;
        util::Id asset;
        Microsoft::Xna::Framework::Vector3 position;
        float yawDeg = 0.0F;
        util::Id lodGroup;
        float impostorFrom = 0.0F;
    };

    struct VegetationInstance
    {
        Microsoft::Xna::Framework::Vector3 position;
        float yawDeg = 0.0F;
        float scale = 1.0F;
    };

    struct VegetationGroup
    {
        util::Id id;
        util::Id asset;
        std::vector<VegetationInstance> instances;
    };

    /// @brief One row of an interactable's `actions`.
    struct InteractableAction
    {
        std::string verb;
        /// Parsed at load. An absent `when` is the predicate that is always true.
        Predicate when = Predicate::AlwaysTrue();
        /// Parsed at load. An absent `do` changes nothing, which is right for an action whose
        /// whole effect is a sound.
        Effect effect = Effect::Nothing();
        util::Id sound;
        std::string anim;
        float duration = 0.0F;
    };

    /// @brief `interactables.json`: one of the 640 rows.
    struct Interactable
    {
        util::Id id;
        /// The kind, e.g. `refrigerator`. Finer than §50.4's twelve behaviour classes — the map
        /// from a kind to a behaviour belongs to the interaction framework, not to the loader,
        /// which would otherwise have to be edited to add a 641st row.
        std::string kind;
        util::Id cell;
        util::Id prop;
        Microsoft::Xna::Framework::Vector3 focusPoint;
        Microsoft::Xna::Framework::Vector3 focusNormal;
        float focusRadius = 0.0F;
        Microsoft::Xna::Framework::Vector3 boundsMin;
        Microsoft::Xna::Framework::Vector3 boundsMax;
        std::vector<InteractableAction> actions;
        std::vector<util::Id> childInteractables;
        /// The typed state the actions are parsed against.
        StateTable state;
        /// Exactly the fields the save carries. §65: a field not listed is derived or transient
        /// and is recomputed on load, which is what keeps a save at ~90 KB.
        std::vector<std::string> persist;
        util::Id audioLoop;
        Microsoft::Xna::Framework::Vector3 audioEmitter;
        util::Id portal;
    };

    /// @brief `initialstate.json`: where the player starts.
    struct PlayerStart
    {
        util::Id cell;
        Microsoft::Xna::Framework::Vector3 position;
        float yawDeg = 0.0F;
    };

    /// @brief `initialstate.json`: the clock a fresh start begins on.
    struct ClockStart
    {
        /// Seconds since the Unix epoch. A double, not a float: at 60x a float loses the second
        /// hand before the first in-game week is out.
        ///
        /// Seconds since the epoch and not seconds into the day, which is what this comment said
        /// until `HOUSE-00395`: §65.6 starts the game on "Saturday 14 June 2031, 09:20", and a
        /// calendar date -- which the season, the moon phase and the sun's position all need -- is
        /// not expressible in an offset into a day.
        double epochSeconds = 0.0;
        float timeScale = 1.0F;
        float latitudeDeg = 0.0F;
        float longitudeDeg = 0.0F;
        std::int32_t utcOffsetMinutes = 0;
    };

    /// @brief `initialstate.json`: the weather a fresh start begins in.
    struct WeatherStart
    {
        util::Id target;
        weather::WeatherState state;
        /// @brief Simulated minutes until §42.1 chooses another target.
        float targetExpiryMinutes = 0.0F;
    };

    /// @brief One interactable's opening state: only the fields the file names.
    ///
    /// A partial table on purpose. `interactables.json` already declares every field and its
    /// default; this says which of them start somewhere else, and a row that repeated all of them
    /// would be a second place to change a default.
    struct InteractableStart
    {
        util::Id id;
        StateTable overrides;
    };

    struct PetStart
    {
        util::Id id;
        util::Id cell;
        std::string state;
    };

    /// @brief `initialstate.json`, whole (`cna-house.md` §65.6).
    ///
    /// The reference every delta save is taken against and every *Reset House* returns to, which
    /// is why it is validated as strictly as the layout.
    struct InitialState
    {
        PlayerStart player;
        ClockStart clock;
        WeatherStart weather;
        std::vector<InteractableStart> interactables;
        std::vector<PetStart> pets;
    };

    /// @brief `layout.exterior.json`, whole.
    struct Exterior
    {
        Terrain terrain;
        Road road;
        std::vector<Fence> fences;
        std::vector<NeighbourBuilding> neighbourhood;
        std::vector<VegetationGroup> vegetation;
    };

} // namespace cnahouse::world
