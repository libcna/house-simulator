// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cnahouse/rendering/RenderStates.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Result.hpp"

namespace Microsoft::Xna::Framework
{
    struct Matrix;
}

namespace Microsoft::Xna::Framework::Graphics
{
    class AlphaTestEffect;
    class BasicEffect;
    class DualTextureEffect;
    class Effect;
    class GraphicsDevice;
    class SkinnedEffect;
    class Texture2D;
} // namespace Microsoft::Xna::Framework::Graphics

namespace cnahouse::rendering
{

    /// @brief Which stock XNA effect a material is drawn with.
    ///
    /// **Four, and this list is closed for Tier S.** ADR-0003 promises Tier S is complete using
    /// stock effects only, and phase 1 measured all four end to end against analytic expectations
    /// (`HOUSE-00078`, `HOUSE-00080`, `HOUSE-00082`, `HOUSE-00075`/`HOUSE-00077`). A fifth kind
    /// would be a fifth thing to measure; adding one is a decision, not a convenience.
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

        /// @brief Content asset names. Empty means "no texture", which is legal and draws untextured.
        std::string diffuseTexture;
        /// @brief The second UV channel's texture. `DualTexture` only.
        std::string secondTexture;

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
        /// @brief Per-pixel where the effect supports it. `BasicEffect` and `SkinnedEffect` only.
        bool perPixelLighting = true;

        /// @brief `AlphaTest` only: the reference value, 0..255.
        ///
        /// MEASURED (`HOUSE-00080`): the cutoff is exact to one alpha value for all six comparison
        /// functions, so a reference of 128 with `Greater` keeps alpha 129 and drops 128 — there is
        /// no half-texel of tolerance to lean on.
        int referenceAlpha = 128;
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
        Microsoft::Xna::Framework::Graphics::Texture2D* second = nullptr;
        /// @brief `Skinned` only. Skin-local, and at most `SkinnedEffect::MaxBones`.
        const std::vector<Microsoft::Xna::Framework::Matrix>* bones = nullptr;
    };

    /// @brief Material id → effect instance with that material's parameters applied.
    ///
    /// **One effect instance per KIND, not per material.** An XNA effect object holds the parameter
    /// values that were last written to it, so the parameters are set per draw whatever happens; an
    /// instance per material would therefore buy nothing and cost one shader object per material in
    /// the house. `HOUSE-00106` measured `EffectPass::Apply()` at 0.184 µs against a draw call at
    /// 8.15 µs, so re-writing parameters is not where the frame goes.
    ///
    /// **What it refuses is the point.** Two of phase 1's measurements are enforced here, at
    /// registration, rather than left to fail inside a frame: `SkinnedEffect` refuses
    /// `LightingEnabled = false` (`HOUSE-00077`), and its palette is capped at
    /// `SkinnedEffect::MaxBones == 72` — 72 accepted, 73 throwing. A material that would violate
    /// either is rejected when it is registered, where the error names the material.
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
        /// unbuildable — an unlit `Skinned` material, or a `DualTexture` one with no second texture.
        util::Result<void> Register(util::Id id, MaterialDesc desc);

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

        /// @brief How many times an effect object was actually created. One per kind, at most.
        [[nodiscard]] std::size_t EffectsCreated() const noexcept
        {
            return effectsCreated_;
        }

    private:
        Microsoft::Xna::Framework::Graphics::BasicEffect& BasicFor();
        Microsoft::Xna::Framework::Graphics::DualTextureEffect& DualTextureFor();
        Microsoft::Xna::Framework::Graphics::AlphaTestEffect& AlphaTestFor();
        Microsoft::Xna::Framework::Graphics::SkinnedEffect& SkinnedFor();

        Microsoft::Xna::Framework::Graphics::GraphicsDevice* device_;
        std::unordered_map<util::Id, MaterialDesc> materials_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> basic_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::DualTextureEffect> dualTexture_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::AlphaTestEffect> alphaTest_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::SkinnedEffect> skinned_;
        std::size_t effectsCreated_ = 0;
    };

} // namespace cnahouse::rendering
