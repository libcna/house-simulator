// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/rendering/RenderStates.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Result.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class AlphaTestEffect;
    class BasicEffect;
    class DualTextureEffect;
    class Effect;
    class EnvironmentMapEffect;
    class GraphicsDevice;
    class SkinnedEffect;
    class Texture2D;
    class TextureCube;
} // namespace Microsoft::Xna::Framework::Graphics

namespace cnahouse::world
{
    struct MaterialDef;
}

namespace cnahouse::rendering
{

    /// @brief Which stock XNA effect a material is drawn with.
    ///
    /// **Four primary kinds, and this list is closed for Tier S.** ADR-0003 promises Tier S is
    /// complete using stock effects only, and phase 1 measured all four end to end against analytic
    /// expectations (`HOUSE-00078`, `HOUSE-00080`, `HOUSE-00082`, `HOUSE-00075`/`HOUSE-00077`).
    /// `EnvironmentMapEffect` was measured too (`HOUSE-00081`), but §22.2 defines it as an extra
    /// reflection pass over one of these materials; `BindEnvironmentMap` represents that distinction.
    enum class MaterialKind
    {
        /// @brief `BasicEffect`. Lit geometry with one texture. The default.
        Basic,
        /// @brief `DualTextureEffect`. Albedo × lightmap on two UV channels — the room shell.
        DualTexture,
        /// @brief `AlphaTestEffect`. Foliage, fences, grilles: a hard cutoff, no sorting.
        AlphaTest,
        /// @brief `SkinnedEffect`. Pets and the avatar.
        Skinned,
    };

    [[nodiscard]] std::string_view MaterialKindName(MaterialKind kind) noexcept;

    /// @brief What a material IS, independent of any device. Loaded from data, never hard-coded.
    struct MaterialDesc
    {
        MaterialKind kind = MaterialKind::Basic;

        /// @brief Content asset name. An empty name is legal only for an untextured `Basic` draw.
        std::string diffuseTexture;

        /// @brief Linear RGB in 0..1, and **already premultiplied where alpha applies**.
        ///
        /// MEASURED (`HOUSE-00065`): the content pipeline premultiplies alpha by default and
        /// `SpriteBatch::Begin()` selects the premultiplied blend, so premultiplied is this
        /// project's one convention. A second premultiply in a material would darken every edge.
        float diffuse[3] = {1.0f, 1.0f, 1.0f};
        float alpha = 1.0f;

        float specularColour[3] = {0.0f, 0.0f, 0.0f};
        float specularPower = 16.0f;

        /// @brief Foliage cards, glass panes seen from behind, the sky dome.
        bool twoSided = false;
        bool vertexColour = false;
        /// @brief Whether a stock effect evaluates its ambient/directional lights.
        bool lightingEnabled = true;
        /// @brief Per-pixel where the effect supports it. `BasicEffect` and `SkinnedEffect` only.
        bool perPixelLighting = true;

        /// @brief `AlphaTest` only: the reference value, 0..255.
        ///
        /// MEASURED (`HOUSE-00080`): the cutoff is exact to one alpha value for all six comparison
        /// functions, so a reference of 128 with `Greater` keeps alpha 129 and drops 128 — there is
        /// no half-texel of tolerance to lean on.
        int referenceAlpha = 128;
    };

    /// @brief The environment-owned fog parameters for one exterior draw batch (§31.5).
    ///
    /// Their derivation from weather and the horizon belongs to `HOUSE-01650`; the material binder
    /// only carries the resulting stock-XNA values without retaining per-frame environment state.
    struct FogParams
    {
        float colour[3] = {0.0F, 0.0F, 0.0F};
        float start = 0.0F;
        float end = 1.0F;
    };

    /// @brief Maps the live weather scalars onto XNA's linear fog ramp.
    ///
    /// The horizon colour is already evaluated in the camera's view direction by `SkySystem`.
    /// Clear air reaches the camera far plane; precipitation and authored fog density pull a
    /// continuous, non-zero-width ramp toward the camera. Inputs fail closed through clamping so a
    /// transient bad weather sample cannot publish NaNs to a shared stock effect.
    [[nodiscard]] FogParams FogParamsFor(const Microsoft::Xna::Framework::Vector3& horizonColour,
                                         float fogDensity,
                                         float precipitationIntensity,
                                         float farPlane) noexcept;

    /// @brief §31.5's one cell boundary: only `EXT_WORLD` receives atmospheric fog.
    [[nodiscard]] const FogParams* ExteriorFogFor(util::Id cell, const FogParams* fog) noexcept;

    /// @brief Per-pass controls for §22.2's supplemental stock-XNA reflection pass.
    ///
    /// The cube is selected by the reflecting object's placement: four mirrors sharing one
    /// material see four different baked rooms, so it cannot honestly live in `MaterialDesc`.
    struct EnvironmentMapParams
    {
        Microsoft::Xna::Framework::Graphics::TextureCube* cubeMap = nullptr;
        float amount = 1.0F;
        float fresnelFactor = 1.0F;
    };

    /// @brief One explicit stock-XNA directional-light slot for lit object detail.
    ///
    /// A shared BasicEffect constructs with DirectionalLight0 enabled at white (1,1,1). Leaving
    /// it untouched makes a dark room's furniture look self-lit. The caller must supply a live
    /// cell-derived light; null means no directional contribution, never the constructor default.
    struct StockDirectionalLight
    {
        Microsoft::Xna::Framework::Vector3 direction{0.0F, -1.0F, 0.0F};
        Microsoft::Xna::Framework::Vector3 diffuse{0.0F, 0.0F, 0.0F};
        Microsoft::Xna::Framework::Vector3 specular{0.0F, 0.0F, 0.0F};
    };

    /// @brief The per-draw values a material cannot know: where the thing is and where it is seen from.
    struct DrawParams
    {
        const Microsoft::Xna::Framework::Matrix* world = nullptr;
        const Microsoft::Xna::Framework::Matrix* view = nullptr;
        const Microsoft::Xna::Framework::Matrix* projection = nullptr;
        /// Non-const because XNA's `setTextureProperty` takes a non-const pointer, and a `const_cast`
        /// in the binder would hide that fact at the one place a reader would look for it.
        Microsoft::Xna::Framework::Graphics::Texture2D* diffuse = nullptr;
        /// @brief `DualTexture` only: the room/light-group lightmap selected for this pass.
        Microsoft::Xna::Framework::Graphics::Texture2D* lightmap = nullptr;
        /// @brief Per-pass colour multiplied by the material tint before it reaches the effect.
        ///
        /// For a lightmapped draw this is the selected bake's colour/intensity (including the
        /// measured HDR scale). Keeping it draw-owned is essential: one wall material is shared by
        /// many cells whose daylight and switch state differ.
        Microsoft::Xna::Framework::Vector3 colourMultiplier{1.0F, 1.0F, 1.0F};
        /// @brief `Basic`/`Skinned` ambient term for non-lightmapped detail geometry.
        Microsoft::Xna::Framework::Vector3 ambientLight{0.0F, 0.0F, 0.0F};
        /// @brief `Basic`/`Skinned`: key, fill and bounce in XNA slot order.
        ///
        /// A missing slot is explicitly disabled. This is important for the shared effect objects:
        /// no later draw may inherit a light written for an earlier room or actor.
        std::array<std::optional<StockDirectionalLight>, 3> directionalLights;
        /// Null disables fog. Non-null enables it with the supplied environment-owned values.
        const FogParams* fog = nullptr;
        /// @brief `Skinned` only. Skin-local, and at most `SkinnedEffect::MaxBones`.
        const std::vector<Microsoft::Xna::Framework::Matrix>* bones = nullptr;
    };

    /// @brief Material id → effect instance with that material's parameters applied.
    ///
    /// **One effect instance per effect class, not per material.** An XNA effect object holds the
    /// parameter values that were last written to it, so the parameters are set per draw whatever
    /// happens; an instance per material would therefore buy nothing and cost one shader object per
    /// material in the house. `HOUSE-00106` measured `EffectPass::Apply()` at 0.184 µs against a draw
    /// call at 8.15 µs, so re-writing parameters is not where the frame goes.
    ///
    /// **What it refuses is the point.** Two of phase 1's measurements are enforced at their honest
    /// boundaries rather than left to throw inside an effect: registration refuses a `SkinnedEffect`
    /// material with `LightingEnabled = false` (`HOUSE-00077`), while `Bind` refuses a draw palette
    /// beyond `SkinnedEffect::MaxBones == 72` — 72 accepted, 73 throwing in the measured API.
    class MaterialBinder
    {
    public:
        /// @brief `SkinnedEffect::MaxBones`, MEASURED by `HOUSE-00077` (72 accepted, 73 throws).
        ///
        /// Restated here as a named constant so a call site does not have to include
        /// `SkinnedEffect.hpp` to know the limit it must respect.
        static constexpr std::size_t kMaxBones = 72;

        /// Both the constructor and the destructor are defined in the .cpp, not here. The effect
        /// members are `unique_ptr`s to forward-declared XNA types, and an inline constructor needs
        /// their complete types for the exception path that unwinds a partly built object -- which
        /// would drag `BasicEffect`, `SkinnedEffect` and the rest into every translation unit that
        /// merely names a material.
        explicit MaterialBinder(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device) noexcept;

        ~MaterialBinder();

        MaterialBinder(const MaterialBinder&) = delete;
        MaterialBinder& operator=(const MaterialBinder&) = delete;

        /// @brief Registers @p desc under @p id.
        ///
        /// Fails on a duplicate id, and on a description this project has measured to be
        /// unbuildable — an unlit `Skinned` material.
        util::Result<void> Register(util::Id id, MaterialDesc desc);

        /// @brief Converts and registers one loaded `layout.materials.json` definition.
        ///
        /// The lightmap is deliberately absent from `MaterialDesc`: §23 supplies it per draw from
        /// the visible room/light group, so it is not a property of a material definition.
        util::Result<void> Register(const world::MaterialDef& definition);

        /// @brief Registers a complete loaded material table atomically.
        ///
        /// On the first bad row, every row inserted by this call is removed again. Registrations
        /// that predate the call are left untouched.
        util::Result<void> RegisterAll(std::span<const world::MaterialDef> definitions);

        [[nodiscard]] const MaterialDesc* Find(util::Id id) const noexcept;

        [[nodiscard]] std::size_t Count() const noexcept
        {
            return materials_.size();
        }

        /// @brief The cull state @p id wants under a world matrix of determinant @p determinant.
        [[nodiscard]] util::Result<CullPolicy> CullFor(util::Id id, float determinant) const noexcept;

        /// @brief The effect for @p id, with the material's parameters and @p draw applied.
        ///
        /// The returned effect is owned by the binder and is only valid until the next `Bind` of the
        /// same kind, which is the honest lifetime: it is one shared instance whose parameters have
        /// just been overwritten.
        util::Result<Microsoft::Xna::Framework::Graphics::Effect*> Bind(util::Id id, const DrawParams& draw);

        /// @brief The supplemental `EnvironmentMapEffect` pass for @p id (§22.2, §59).
        ///
        /// Environment mapping is deliberately not a fifth `MaterialKind`: chrome and mirror
        /// reflections are extra passes over a Basic material, and glass keeps its transparent
        /// Basic pass as well. The baked cube is placement-owned and supplied in @p environment.
        util::Result<Microsoft::Xna::Framework::Graphics::Effect*>
        BindEnvironmentMap(util::Id id, const DrawParams& draw, const EnvironmentMapParams& environment);

        /// @brief How many effect objects were actually created. One per effect class, at most.
        [[nodiscard]] std::size_t EffectsCreated() const noexcept
        {
            return effectsCreated_;
        }

    private:
        Microsoft::Xna::Framework::Graphics::BasicEffect& BasicFor();
        Microsoft::Xna::Framework::Graphics::DualTextureEffect& DualTextureFor();
        Microsoft::Xna::Framework::Graphics::AlphaTestEffect& AlphaTestFor();
        Microsoft::Xna::Framework::Graphics::SkinnedEffect& SkinnedFor();
        Microsoft::Xna::Framework::Graphics::EnvironmentMapEffect& EnvironmentMapFor();

        Microsoft::Xna::Framework::Graphics::GraphicsDevice* device_;
        std::unordered_map<util::Id, MaterialDesc> materials_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> basic_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::DualTextureEffect> dualTexture_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::AlphaTestEffect> alphaTest_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::SkinnedEffect> skinned_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::EnvironmentMapEffect> environmentMap_;
        std::size_t effectsCreated_ = 0;

        /// The 72-matrix skinning palette, allocated once and refilled per draw.
        ///
        /// MEASURED (`HOUSE-00029`): a fresh `std::vector` of 72 matrices costs **149 ns** against
        /// **107 ns** to refill a reused one -- 42 ns of pure allocation on a path that runs per
        /// skinned draw. Against `HOUSE-00106`'s 8.15 us draw call that is 0.5 %, which is small;
        /// it is removed anyway because a member vector is one line and there is nothing to weigh
        /// against it. It is NOT a `SmallVector`: 72 x 64 B is 4 608 B, which belongs on the heap
        /// once rather than on the stack every call.
        std::vector<Microsoft::Xna::Framework::Matrix> palette_;
    };

} // namespace cnahouse::rendering
