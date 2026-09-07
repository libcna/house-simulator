// SPDX-License-Identifier: MIT
#include "cnahouse/world/WorldTypes.hpp"

#include <array>
#include <string>
#include <utility>

/// @file
/// The vocabularies of `docs/world-format.md`, in both directions.
///
/// Each table is written once and both functions read it, so a value added to the enum and to the
/// spelling table cannot be parseable but unprintable, or the reverse. `WorldDataTests` walks
/// every table and requires `Parse(ToStringView(v)) == v` for every value, which is what makes
/// "written once" true rather than intended.

namespace cnahouse::world
{
    namespace
    {
        template<typename Enum, std::size_t N>
        [[nodiscard]] std::string_view Spell(const std::array<std::pair<Enum, std::string_view>, N>& table,
                                             Enum value) noexcept
        {
            for (const auto& [entry, text] : table)
            {
                if (entry == value)
                {
                    return text;
                }
            }
            return "?";
        }

        template<typename Enum, std::size_t N>
        [[nodiscard]] util::Result<Enum> Read(const std::array<std::pair<Enum, std::string_view>, N>& table,
                                              std::string_view text,
                                              std::string_view what)
        {
            for (const auto& [entry, spelling] : table)
            {
                if (spelling == text)
                {
                    return entry;
                }
            }

            // The message lists the whole vocabulary. An authoring mistake in a closed enum is
            // almost always a spelling or a value from a neighbouring field, and both are answered
            // by seeing the alternatives rather than by being told the value was wrong.
            std::string allowed;
            for (const auto& [entry, spelling] : table)
            {
                (void)entry;
                if (!allowed.empty())
                {
                    allowed += " | ";
                }
                allowed += spelling;
            }
            return util::Err(util::ErrorCode::InvalidData,
                             std::string("'") + std::string(text) + "' is not a " + std::string(what) +
                                 "; expected one of " + allowed);
        }

        constexpr std::array<std::pair<CellKind, std::string_view>, 7> kCellKinds{{
            {CellKind::Room, "room"},
            {CellKind::Corridor, "corridor"},
            {CellKind::Stair, "stair"},
            {CellKind::Closet, "closet"},
            {CellKind::Garage, "garage"},
            {CellKind::Exterior, "exterior"},
            {CellKind::Void, "void"},
        }};

        constexpr std::array<std::pair<PortalKind, std::string_view>, 9> kPortalKinds{{
            {PortalKind::CasedOpening, "cased_opening"},
            {PortalKind::Door, "door"},
            {PortalKind::DoubleDoor, "double_door"},
            {PortalKind::Slider, "slider"},
            {PortalKind::Window, "window"},
            {PortalKind::GarageDoor, "garage_door"},
            {PortalKind::StairWell, "stair_well"},
            {PortalKind::ExteriorDoor, "exterior_door"},
            {PortalKind::Hatch, "hatch"},
        }};

        constexpr std::array<std::pair<PortalOpacity, std::string_view>, 4> kOpacities{{
            {PortalOpacity::Open, "open"},
            {PortalOpacity::OpaqueWhenClosed, "opaque_when_closed"},
            {PortalOpacity::Translucent, "translucent"},
            {PortalOpacity::Glass, "glass"},
        }};

        constexpr std::array<std::pair<PlaneAxis, std::string_view>, 3> kAxes{{
            {PlaneAxis::X, "x"},
            {PlaneAxis::Y, "y"},
            {PlaneAxis::Z, "z"},
        }};

        constexpr std::array<std::pair<OpeningKind, std::string_view>, 2> kOpeningKinds{{
            {OpeningKind::Door, "door"},
            {OpeningKind::Window, "window"},
        }};

        constexpr std::array<std::pair<HingeSide, std::string_view>, 2> kHinges{{
            {HingeSide::Left, "left"},
            {HingeSide::Right, "right"},
        }};

        constexpr std::array<std::pair<LightType, std::string_view>, 5> kLightTypes{{
            {LightType::Point, "point"},
            {LightType::Spot, "spot"},
            {LightType::Directional, "directional"},
            {LightType::AreaProxy, "area_proxy"},
            {LightType::EmissiveOnly, "emissive_only"},
        }};

        constexpr std::array<std::pair<AlphaMode, std::string_view>, 3> kAlphaModes{{
            {AlphaMode::Opaque, "opaque"},
            {AlphaMode::Mask, "mask"},
            {AlphaMode::Blend, "blend"},
        }};

        constexpr std::array<std::pair<EffectTier, std::string_view>, 4> kEffectTiers{{
            {EffectTier::Basic, "Basic"},
            {EffectTier::DualTexture, "DualTexture"},
            {EffectTier::AlphaTest, "AlphaTest"},
            {EffectTier::Skinned, "Skinned"},
        }};

        constexpr std::array<std::pair<PropCollision, std::string_view>, 3> kPropCollisions{{
            {PropCollision::Proxy, "proxy"},
            {PropCollision::None, "none"},
            {PropCollision::Box, "box"},
        }};

        constexpr std::array<std::pair<VisibilityHint, std::string_view>, 2> kVisibilityHints{{
            {VisibilityHint::Opaque, "opaque"},
            {VisibilityHint::Open, "open"},
        }};

        constexpr std::array<std::pair<Orientation, std::string_view>, 8> kOrientations{{
            {Orientation::N, "N"},
            {Orientation::NE, "NE"},
            {Orientation::E, "E"},
            {Orientation::SE, "SE"},
            {Orientation::S, "S"},
            {Orientation::SW, "SW"},
            {Orientation::W, "W"},
            {Orientation::NW, "NW"},
        }};

        constexpr std::array<std::pair<MarkerKind, std::string_view>, 3> kMarkerKinds{{
            {MarkerKind::Perch, "perch"},
            {MarkerKind::Bed, "bed"},
            {MarkerKind::Bowl, "bowl"},
        }};

        constexpr std::array<std::pair<MaterialClass, std::string_view>, 20> kMaterialClasses{{
            {MaterialClass::Paint, "paint"},     {MaterialClass::Wood, "wood"},
            {MaterialClass::Carpet, "carpet"},   {MaterialClass::Tile, "tile"},
            {MaterialClass::Stone, "stone"},     {MaterialClass::Concrete, "concrete"},
            {MaterialClass::Metal, "metal"},     {MaterialClass::Plastic, "plastic"},
            {MaterialClass::Glass, "glass"},     {MaterialClass::Fabric, "fabric"},
            {MaterialClass::Skin, "skin"},       {MaterialClass::Hair, "hair"},
            {MaterialClass::Fur, "fur"},         {MaterialClass::Foliage, "foliage"},
            {MaterialClass::Asphalt, "asphalt"}, {MaterialClass::Gravel, "gravel"},
            {MaterialClass::Grass, "grass"},     {MaterialClass::Soil, "soil"},
            {MaterialClass::Water, "water"},     {MaterialClass::Emissive, "emissive"},
        }};

        constexpr std::array<std::pair<SurfaceState, std::string_view>, 3> kSurfaceStates{{
            {SurfaceState::Dry, "dry"},
            {SurfaceState::Wet, "wet"},
            {SurfaceState::Snowy, "snow"},
        }};

        constexpr std::array<std::pair<Species, std::string_view>, 2> kSpecies{{
            {Species::Dog, "dog"},
            {Species::Cat, "cat"},
        }};
    } // namespace

    std::string_view ToStringView(CellKind value) noexcept
    {
        return Spell(kCellKinds, value);
    }

    std::string_view ToStringView(PortalKind value) noexcept
    {
        return Spell(kPortalKinds, value);
    }

    std::string_view ToStringView(PortalOpacity value) noexcept
    {
        return Spell(kOpacities, value);
    }

    std::string_view ToStringView(PlaneAxis value) noexcept
    {
        return Spell(kAxes, value);
    }

    std::string_view ToStringView(OpeningKind value) noexcept
    {
        return Spell(kOpeningKinds, value);
    }

    std::string_view ToStringView(HingeSide value) noexcept
    {
        return Spell(kHinges, value);
    }

    std::string_view ToStringView(LightType value) noexcept
    {
        return Spell(kLightTypes, value);
    }

    std::string_view ToStringView(AlphaMode value) noexcept
    {
        return Spell(kAlphaModes, value);
    }

    std::string_view ToStringView(EffectTier value) noexcept
    {
        return Spell(kEffectTiers, value);
    }

    std::string_view ToStringView(PropCollision value) noexcept
    {
        return Spell(kPropCollisions, value);
    }

    std::string_view ToStringView(VisibilityHint value) noexcept
    {
        return Spell(kVisibilityHints, value);
    }

    std::string_view ToStringView(Orientation value) noexcept
    {
        return Spell(kOrientations, value);
    }

    std::string_view ToStringView(MarkerKind value) noexcept
    {
        return Spell(kMarkerKinds, value);
    }

    util::Result<CellKind> ParseCellKind(std::string_view text)
    {
        return Read(kCellKinds, text, "cell kind");
    }

    util::Result<PortalKind> ParsePortalKind(std::string_view text)
    {
        return Read(kPortalKinds, text, "portal kind");
    }

    util::Result<PortalOpacity> ParsePortalOpacity(std::string_view text)
    {
        return Read(kOpacities, text, "portal opacity");
    }

    util::Result<PlaneAxis> ParsePlaneAxis(std::string_view text)
    {
        return Read(kAxes, text, "plane axis");
    }

    util::Result<OpeningKind> ParseOpeningKind(std::string_view text)
    {
        return Read(kOpeningKinds, text, "opening kind");
    }

    util::Result<HingeSide> ParseHingeSide(std::string_view text)
    {
        return Read(kHinges, text, "hinge side");
    }

    util::Result<LightType> ParseLightType(std::string_view text)
    {
        return Read(kLightTypes, text, "light type");
    }

    util::Result<AlphaMode> ParseAlphaMode(std::string_view text)
    {
        return Read(kAlphaModes, text, "alpha mode");
    }

    util::Result<EffectTier> ParseEffectTier(std::string_view text)
    {
        return Read(kEffectTiers, text, "Tier S effect");
    }

    util::Result<PropCollision> ParsePropCollision(std::string_view text)
    {
        return Read(kPropCollisions, text, "prop collision");
    }

    util::Result<VisibilityHint> ParseVisibilityHint(std::string_view text)
    {
        return Read(kVisibilityHints, text, "visibility hint");
    }

    util::Result<Orientation> ParseOrientation(std::string_view text)
    {
        return Read(kOrientations, text, "compass orientation");
    }

    util::Result<Species> ParseSpecies(std::string_view text)
    {
        return Read(kSpecies, text, "species");
    }

    std::string_view ToStringView(MaterialClass value) noexcept
    {
        return Spell(kMaterialClasses, value);
    }

    std::string_view ToStringView(SurfaceState value) noexcept
    {
        return Spell(kSurfaceStates, value);
    }

    util::Result<MaterialClassSpec> ParseMaterialClass(std::string_view text)
    {
        MaterialClassSpec spec;
        std::string_view base = text;
        if (base.starts_with("wet_"))
        {
            spec.state = SurfaceState::Wet;
            base.remove_prefix(4);
        }
        else if (base.starts_with("snow_"))
        {
            spec.state = SurfaceState::Snowy;
            base.remove_prefix(5);
        }

        const util::Result<MaterialClass> parsed = Read(kMaterialClasses, base, "material class");
        if (!parsed)
        {
            // The message must quote what was AUTHORED, not the stem left after the prefix was
            // taken off: `wet_marble` is the typo, and telling the author that `marble` is not a
            // class sends them looking for a field that does not say that.
            return util::Err(util::ErrorCode::InvalidData,
                             std::string("'") + std::string(text) + "' is not a §22.2 material class; " +
                                 parsed.Error().Message());
        }
        spec.base = parsed.Value();
        return spec;
    }

    std::string SpellMaterialClass(MaterialClassSpec spec)
    {
        std::string text;
        if (spec.state == SurfaceState::Wet)
        {
            text = "wet_";
        }
        else if (spec.state == SurfaceState::Snowy)
        {
            text = "snow_";
        }
        text += ToStringView(spec.base);
        return text;
    }

    EffectTier DefaultEffectTier(MaterialClass value) noexcept
    {
        switch (value)
        {
            case MaterialClass::Paint:
            case MaterialClass::Wood:
            case MaterialClass::Carpet:
            case MaterialClass::Tile:
            case MaterialClass::Stone:
            case MaterialClass::Concrete:
            case MaterialClass::Asphalt:
            case MaterialClass::Gravel:
            case MaterialClass::Grass:
            case MaterialClass::Soil:
                return EffectTier::DualTexture;
            case MaterialClass::Foliage:
            case MaterialClass::Hair:
                return EffectTier::AlphaTest;
            case MaterialClass::Skin:
            case MaterialClass::Fur:
                // §22.2 draws both with `SkinnedEffect`. `build_chunks.py` refuses to batch a
                // static prop wearing one rather than choosing a layout, because a skinned prop is
                // an animated prop and batching it would freeze it in its bind pose inside a wall.
                return EffectTier::Skinned;
            case MaterialClass::Metal:
            case MaterialClass::Plastic:
            case MaterialClass::Glass:
            case MaterialClass::Fabric:
            case MaterialClass::Water:
            case MaterialClass::Emissive:
                return EffectTier::Basic;
        }
        return EffectTier::Basic;
    }

} // namespace cnahouse::world
